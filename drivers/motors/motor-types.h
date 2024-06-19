/*
 * Base Driver for ice40 Accessory Communication Interface
 *
 * Copyright (c) 2020 LG Electronics, Inc
 *
 * All rights are reserved.
 *
 * IMPORTANT - PLEASE READ CAREFULLY BEFORE COPYING, INSTALLING OR USING
 * THE SOFTWARE.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

/* Motor Position */
enum motor_position {
	MOTOR_POSITION_COLLAPSE = 1,
	MOTOR_POSITION_DISABLE  = 2,
	MOTOR_POSITION_EXPAND   = 3,
	MOTOR_POSITION_16_9     = 4,
};

/* Motor Control State */
enum motor_state {                 // DEC       HEX       BIN
	MOTOR_STATE_FORCE_DISABLE       = -2,
	MOTOR_STATE_BACKWARD_DISABLE    =  0,    //  0     0000 0000
	MOTOR_STATE_BACKWARD_ENABLE     =  1,    //  1     0000 0001
	MOTOR_STATE_FORWARD_DISABLE     =  2,    //  2     0000 0010
	MOTOR_STATE_FORWARD_ENABLE      =  3,    //  3     0000 0011
};

enum motor_status_mask {
	MOTOR_ENABLE_MASK       = 1,            //  1    0000 0001   0 : disable ,  1 : enable
	MOTOR_DIRECTION_MASK    = 2,            //  2    0000 0010   0 : backward,  1 : forward
	MOTOR_INSTANT_MOVE_MASK = 4,            //  3    0000 0100   0 : normal  ,  1 : exception
};

/* Motor Max Frequency Accoring Temperature */
enum motor_temperature_section {            // Temperature  Index     PPS        Time      Value
	MOTOR_TEMP_BELOW_M_5    = 1,            //      ~ -5'C    1      500PPS     9.60Sec      8
	MOTOR_TEMP_M_5_P_5      = 2,            // -5'C ~  5'C    2     1400PPS     3.88Sec      6
	MOTOR_TEMP_P_5_P_10     = 3,            //  5'C ~ 10'C    3     1800PPS     3.30Sec      4
	MOTOR_TEMP_P_10_P_15    = 4,            // 10'C ~ 15'C    4     2200PPS     2.85Sec      2
	MOTOR_TEMP_ABOVE_P_15   = 5,            // 15'C ~         5     2400PPS     2.70Sec      0
};

/* Motor PWM State */
enum motor_pwm {
	MOTOR_PWM_DISABLE  = 0,
	MOTOR_PWM_ENABLE   = 1,
};

/* Motor Lreset State */
enum motor_lattice {
	MOTOR_LRESET_DISABLE = 0,
	MOTOR_LRESET_ENABLE  = 1,
};

/* Motor Operation Type */
enum motor_operation_type { // MotorOperationType : int32_t (types.hal)
	MOTOR_NORMAL    = 0,
	MOTOR_TRY_AGAIN = 1,
	MOTOR_BACK      = 2,
};

/* Motor Frequency Set */
enum motor_frequency_time {
	MOTOR_F_TIME_0  = 0,
	MOTOR_F_TIME_1  = 1,
	MOTOR_F_TIME_2  = 2,
	MOTOR_F_TIME_3  = 3,
	MOTOR_F_TIME_4  = 4,
	MOTOR_F_TIME_5  = 5,
	MOTOR_F_TIME_6  = 6,
	MOTOR_F_TIME_7  = 7,
	MOTOR_F_TIME_8  = 8,
	MOTOR_F_MIN_D   = 9,
};

enum motor_error {
	MOTOR_SUCCESS         = 0,    /* Success */
	MOTOR_EPERM           = 1,    /* Operation not permitted */
	MOTOR_ENOENT          = 2,    /* No such file or directory */
	MOTOR_ESRCH		      = 3,    /* No such process */
	MOTOR_EINTR		      = 4,    /* Interrupted system call */
	MOTOR_EIO             = 5,    /* I/O error */
	MOTOR_ENXIO		      = 6,	  /* No such device or address */
	MOTOR_EAGAIN          = 11,   /* Try again */
	MOTOR_ENOMEM          = 12,   /* Out of memory */
	MOTOR_EACCES    	  = 13,   /* Permission denied */
	MOTOR_EFAULT          = 14,   /* Bad address */
	MOTOR_EBUSY           = 16,   /* Device or resource busy */
	MOTOR_EEXIST          = 17,   /* File exists */
	MOTOR_ENODEV	      = 19,   /* No such device */
	MOTOR_ENOTDIR         = 20,   /* Not a directory */
	MOTOR_EINVAL          = 22,   /* Invalid argument */
	MOTOR_EROFS           = 30,   /* Read-only file system */
	MOTOR_CANCELED        = 125,  /* Operation Canceled */
	MOTOR_ENOTRECOVERABLE = 131,  /* State not recoverable */
	MOTOR_SAMEVALUE       = 140,  /* Same Value */
};