/*
 * Copyright (C) 2020 Peter Sjödin, KTH
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */


/*
 * Macros for writing sensor records 
 * A record is a unit of text that must be kept together 
 * in a report, ie to represent a senml element
 */

//#define WARN printf
#define WARN(...)

   
/*
 * Mark start of a record. 
 * Save current state as starting state (string pointer and length).
 */
#define RECORD_START(STR, LEN) {                                        \
   __label__ _full, _notfull;                                           \
   char *_str = (STR);                                                  \
   size_t _len = (LEN), _nread = 0;                                     \

/*
 * Attempt to write printf-formatted text to a record
 */

#define PUTFMT(...) {                                                   \
    size_t _n = snprintf(_str + _nread, _len - _nread, __VA_ARGS__);    \
    if (_n < _len - _nread) {                                           \
        _nread += _n;                                                   \
    }                                                                   \
    else {                                                              \
         goto _full;                                                    \
    }                                                                   \
  }

/*
 * End of record -- commit if succeeded, else abort and revert to
 * starting state.
 */
#define RECORD_END(NREAD)                                               \
   goto _notfull;                                                       \
  _full:                                                                \
    WARN("%s: %d: no space left\n", __FILE__, __LINE__);                \
    *(_str) = '\0';                                                     \
    return ((NREAD));                                                   \
  _notfull:                                                             \
  (NREAD) += _nread;                                                    \
  }

/*
 * For calling external functions to place data in buffer:
 *
 * - Get current buffer pointer and len
 */
#define RECORD_STR() (_str + _nread)
#define RECORD_LEN() (_len - _nread)
/*
 * - Advance no of bytes written to buffer by N
 */
#define RECORD_ADD(N)                                                   \
  if ((N) < _len - _nread) {                                            \
      _nread += (N);                                                    \
   }                                                                    \
   else {                                                               \
      goto _full;                                                       \
   }                                                                    \
   

typedef int (* report_gen_t)(uint8_t *buf, size_t len, uint8_t *finished);

