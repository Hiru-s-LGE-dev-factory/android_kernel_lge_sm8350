#ifndef __LGE_NFC_MATCHINGLIST_H__
#define __LGE_NFC_MATCHINGLIST_H__

#include "lgenfc.h"

enum NfcChip mMatchingDefault = PN553;

struct matching_t mMatchingList[] = {
#ifdef CONFIG_MACH_LAHAINA_BLM
  {SN100X, HW_SKU_JP}
#elif defined(CONFIG_MACH_LAHAINA_RAINBOWLM)
  {SN100X, HW_SKU_JP}
#endif
};

int mMatchingListSize = sizeof(mMatchingList) / sizeof (struct matching_t);

#endif /* __LGE_NFC_MATCHINGLIST_H__ */
