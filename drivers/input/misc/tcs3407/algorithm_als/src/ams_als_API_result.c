/*
*****************************************************************************
* Copyright by ams AG                                                       *
* All rights are reserved.                                                  *
*                                                                           *
* IMPORTANT - PLEASE READ CAREFULLY BEFORE COPYING, INSTALLING OR USING     *
* THE SOFTWARE.                                                             *
*                                                                           *
* THIS SOFTWARE IS PROVIDED FOR USE ONLY IN CONJUNCTION WITH AMS PRODUCTS.  *
* USE OF THE SOFTWARE IN CONJUNCTION WITH NON-AMS-PRODUCTS IS EXPLICITLY    *
* EXCLUDED.                                                                 *
*                                                                           *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         *
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS         *
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT  *
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,     *
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT          *
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     *
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY     *
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT       *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE     *
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.      *
*****************************************************************************
*/

/*
 * Device Algorithm ALS
 */

/*
 * @@AMS_REVISION_Id: 94092f58014f4160671b0fcf30e2d665c6fbc02a
 */

#ifdef CONFIG_AMS_OPTICAL_SENSOR_ALS
#include "../../include/ams_als_API.h"

/*
 * getResult: will return the output generated by the algorithm
 */

int amsAlg_als_getResult(amsAlsContext_t * ctx, amsAlsResult_t * outData){
    int ret = 0;

    outData->rawClear  = ctx->results.rawClear;
    outData->rawRed  = ctx->results.rawRed;
    outData->rawGreen  = ctx->results.rawGreen;
    outData->rawBlue  = ctx->results.rawBlue;
    outData->irrBlue  = ctx->results.irrBlue;
    outData->irrClear = ctx->results.irrClear;
    outData->irrGreen = ctx->results.irrGreen;
    outData->irrRed   = ctx->results.irrRed;
    outData->irrWideband = ctx->results.irrWideband;
    outData->mLux_ave  = ctx->results.mLux_ave / AMS_LUX_AVERAGE_COUNT;
    outData->IR  = ctx->results.IR;
    outData->saturation = ctx->results.saturation;
    outData->adaptive = ctx->results.adaptive;
    if (ctx->notStableMeasurement) {
        ctx->notStableMeasurement = false;
    }
    outData->mLux = ctx->results.mLux;

    return ret;
}
#endif
