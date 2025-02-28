// SPDX-License-Identifier: GPL-2.0
/*
 * ION Memory Allocator generic heap helpers
 *
 * Copyright (C) 2011 Google, Inc.
 */

#include <linux/err.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/rtmutex.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/scatterlist.h>
#include <linux/vmalloc.h>

#include "ion_private.h"

static unsigned long ion_heap_shrink_count(struct shrinker *shrinker,
					   struct shrink_control *sc)
{
	struct ion_heap *heap = container_of(shrinker, struct ion_heap,
					     shrinker);
	int total = 0;

	total = ion_heap_freelist_size(heap) / PAGE_SIZE;

#ifdef CONFIG_MIGRATE_HIGHORDER
	total -= ion_heap_free_highorder_size(heap) / PAGE_SIZE;
#endif

	if (heap->ops->shrink)
		total += heap->ops->shrink(heap, sc->gfp_mask, 0);

	return total;
}

static unsigned long ion_heap_shrink_scan(struct shrinker *shrinker,
					  struct shrink_control *sc)
{
	struct ion_heap *heap = container_of(shrinker, struct ion_heap,
					     shrinker);
	int freed = 0;
	int to_scan = sc->nr_to_scan;

	if (to_scan == 0)
		return 0;

	/*
	 * shrink the free list first, no point in zeroing the memory if we're
	 * just going to reclaim it. Also, skip any possible page pooling.
	 */
	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		freed = ion_heap_freelist_shrink(heap, to_scan * PAGE_SIZE) /
				PAGE_SIZE;

	to_scan -= freed;
	if (to_scan <= 0)
		return freed;

	if (heap->ops->shrink)
		freed += heap->ops->shrink(heap, sc->gfp_mask, to_scan);

	return freed;
}

static size_t _ion_heap_freelist_drain(struct ion_heap *heap, size_t size,
				       bool skip_pools)
{
	struct ion_buffer *buffer;
	size_t total_drained = 0;

	if (ion_heap_freelist_size(heap) == 0)
		return 0;

	spin_lock(&heap->free_lock);
	if (size == 0)
		size = heap->free_list_size;

	while (!list_empty(&heap->free_list)) {
		if (total_drained >= size)
			break;
		buffer = list_first_entry(&heap->free_list, struct ion_buffer,
					  list);
		list_del(&buffer->list);
		heap->free_list_size -= buffer->size;
#ifdef CONFIG_MIGRATE_HIGHORDER
		heap->free_highorder_size -= buffer->highorder_size;
#endif
		if (skip_pools)
			buffer->private_flags |= ION_PRIV_FLAG_SHRINKER_FREE;
		total_drained += buffer->size;
#ifdef CONFIG_MIGRATE_HIGHORDER
		if (skip_pools)
			total_drained -= buffer->highorder_size;
#endif
		spin_unlock(&heap->free_lock);
		ion_buffer_release(buffer);
		spin_lock(&heap->free_lock);
	}
	spin_unlock(&heap->free_lock);

	return total_drained;
}

static int ion_heap_deferred_free(void *data)
{
	struct ion_heap *heap = data;

	while (true) {
		struct ion_buffer *buffer;

		wait_event_freezable(heap->waitqueue,
				     (ion_heap_freelist_size(heap) > 0 ||
				      kthread_should_stop()));

		spin_lock(&heap->free_lock);
		if (list_empty(&heap->free_list)) {
			spin_unlock(&heap->free_lock);
			if (!kthread_should_stop())
				continue;
			break;
		}
		buffer = list_first_entry(&heap->free_list, struct ion_buffer,
					  list);
		list_del(&buffer->list);
		heap->free_list_size -= buffer->size;
#ifdef CONFIG_MIGRATE_HIGHORDER
		heap->free_highorder_size -= buffer->highorder_size;
#endif
		spin_unlock(&heap->free_lock);
		ion_buffer_release(buffer);
	}

	return 0;
}

void *ion_heap_map_kernel(struct ion_heap *heap,
			  struct ion_buffer *buffer)
{
	struct scatterlist *sg;
	int i, j;
	void *vaddr;
	pgprot_t pgprot;
	struct sg_table *table = buffer->sg_table;
	int npages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
	struct page **pages = vmalloc(array_size(npages,
						 sizeof(struct page *)));
	struct page **tmp = pages;

	if (!pages)
		return ERR_PTR(-ENOMEM);

	if (buffer->flags & ION_FLAG_CACHED)
		pgprot = PAGE_KERNEL;
	else
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	for_each_sg(table->sgl, sg, table->nents, i) {
		int npages_this_entry = PAGE_ALIGN(sg->length) / PAGE_SIZE;
		struct page *page = sg_page(sg);

		BUG_ON(i >= npages);
		for (j = 0; j < npages_this_entry; j++)
			*(tmp++) = page++;
	}
	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	vfree(pages);

	if (!vaddr)
		return ERR_PTR(-ENOMEM);

	return vaddr;
}
EXPORT_SYMBOL_GPL(ion_heap_map_kernel);

void ion_heap_unmap_kernel(struct ion_heap *heap,
			   struct ion_buffer *buffer)
{
	vunmap(buffer->vaddr);
}
EXPORT_SYMBOL_GPL(ion_heap_unmap_kernel);

int ion_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
		      struct vm_area_struct *vma)
{
	struct sg_table *table = buffer->sg_table;
	unsigned long addr = vma->vm_start;
	unsigned long offset = vma->vm_pgoff * PAGE_SIZE;
	struct scatterlist *sg;
	int i;
	int ret;

	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page = sg_page(sg);
		unsigned long remainder = vma->vm_end - addr;
		unsigned long len = sg->length;

		if (offset >= sg->length) {
			offset -= sg->length;
			continue;
		} else if (offset) {
			page += offset / PAGE_SIZE;
			len = sg->length - offset;
			offset = 0;
		}
		len = min(len, remainder);
		ret = remap_pfn_range(vma, addr, page_to_pfn(page), len,
				      vma->vm_page_prot);
		if (ret)
			return ret;
		addr += len;
		if (addr >= vma->vm_end)
			return 0;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ion_heap_map_user);

void ion_heap_freelist_add(struct ion_heap *heap, struct ion_buffer *buffer)
{
	spin_lock(&heap->free_lock);
	list_add(&buffer->list, &heap->free_list);
	heap->free_list_size += buffer->size;
#ifdef CONFIG_MIGRATE_HIGHORDER
	heap->free_highorder_size += buffer->highorder_size;
#endif
	spin_unlock(&heap->free_lock);
	wake_up(&heap->waitqueue);
}

size_t ion_heap_freelist_size(struct ion_heap *heap)
{
	size_t size;

	spin_lock(&heap->free_lock);
	size = heap->free_list_size;
	spin_unlock(&heap->free_lock);

	return size;
}

#ifdef CONFIG_MIGRATE_HIGHORDER
size_t ion_heap_free_highorder_size(struct ion_heap *heap)
{
	size_t size;

	spin_lock(&heap->free_lock);
	size = heap->free_highorder_size;
	spin_unlock(&heap->free_lock);

	return size;
}
#endif

size_t ion_heap_freelist_drain(struct ion_heap *heap, size_t size)
{
	return _ion_heap_freelist_drain(heap, size, false);
}

size_t ion_heap_freelist_shrink(struct ion_heap *heap, size_t size)
{
	return _ion_heap_freelist_drain(heap, size, true);
}

int ion_heap_init_deferred_free(struct ion_heap *heap)
{
#ifndef CONFIG_ION_DEFER_FREE_NO_SCHED_IDLE
	struct sched_param param = { .sched_priority = 0 };
#endif
	INIT_LIST_HEAD(&heap->free_list);
	init_waitqueue_head(&heap->waitqueue);
	heap->task = kthread_run(ion_heap_deferred_free, heap,
				 "%s", heap->name);
	if (IS_ERR(heap->task)) {
		pr_err("%s: creating thread for deferred free failed\n",
		       __func__);
		return PTR_ERR_OR_ZERO(heap->task);
	}
#ifndef CONFIG_ION_DEFER_FREE_NO_SCHED_IDLE
	sched_setscheduler(heap->task, SCHED_IDLE, &param);
#endif
	return 0;
}

int ion_heap_init_shrinker(struct ion_heap *heap)
{
	heap->shrinker.count_objects = ion_heap_shrink_count;
	heap->shrinker.scan_objects = ion_heap_shrink_scan;
	heap->shrinker.seeks = DEFAULT_SEEKS;
	heap->shrinker.batch = 0;

	return register_shrinker(&heap->shrinker);
}

int ion_heap_cleanup(struct ion_heap *heap)
{
	int ret;

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE &&
	    !IS_ERR_OR_NULL(heap->task)) {
		size_t free_list_size = ion_heap_freelist_size(heap);
		size_t total_drained = ion_heap_freelist_drain(heap, 0);

		if (total_drained != free_list_size) {
			pr_err("%s: %s heap drained %zu bytes, requested %zu\n",
			       __func__, heap->name, free_list_size,
			       total_drained);
			return -EBUSY;
		}
		ret = kthread_stop(heap->task);
		if (ret < 0) {
			pr_err("%s: failed to stop heap free thread\n",
			       __func__);
			return ret;
		}
	}

	if ((heap->flags & ION_HEAP_FLAG_DEFER_FREE) || heap->ops->shrink)
		unregister_shrinker(&heap->shrinker);

	return 0;
}
