#ifndef COM_SCAN_H
#define COM_SCAN_H

#include "com_define.h"
#include "com_reader.h"
#include "com_writer.h"
#include "com_loc.h"

// TODO add more scan options for integers and doubles

typedef struct {
  // if it encountered the character
  bool successful;
} com_scan_UntilResult;

/// read until `terminator` is encountered in the stream
/// REQUIRES: `destination` is a valid pointer to a valid `com_writer`
/// REQUIRES: `source` is a valid pointer to a valid `com_reader`
/// REQUIRES: `terminator` is a char that will terminate reading of the stream
/// GUARANTEES: will not write `terminator` into the destination
/// GUARANTEES: if a read from source or a write to destination fails, will stop
/// GUARANTEES: is not atomic
/// GUARANTEES: reads until `terminator` 
/// or min(flags(source) & limited ? source.limit : usize_max, flags(destination) & limited ? destination.limit : usize_max)
/// GUARANTEES: returns successful if no unexpected errors encountered
com_scan_UntilResult com_scan_until(com_writer* destination, com_reader* source, u8 terminator);


// All checked strings obey:
// https://tools.ietf.org/html/rfc7159#section-7

typedef enum {
  com_scan_CheckedStrSuccessful,
  com_scan_CheckedStrReadFailed,
  com_scan_CheckedStrInvalidControlChar,
  com_scan_CheckedStrInvalidUnicodeSpecifier,
} com_scan_CheckedStrErrorKind;

// Result of reading a checked str
typedef struct {
    com_scan_CheckedStrErrorKind result;
    com_loc_Span span;
} com_scan_CheckedStrResult;

/// parses checked string until syntax error or character specified is encountered
/// REQUIRES: `destination` is a valid pointer to a valid com_writer
/// REQUIRES: `source` is a valid pointer to a valid com_reader
/// GUARANTEES: will read "characters" from reader until an unescaped `terminator` char is encountered
/// GUARANTEES: will write the unescaped string to destination
/// GUARANTEES: will not write `terminator` into the destination
/// GUARANTEES: if a syntax error is encountered, will immediately halt reading
/// GUARANTEES: this operation is not atomic
/// GUARANTEES: will ignore any write failures
com_scan_CheckedStrResult com_scan_checked_str_until(com_writer* destination, com_reader *source, u8 terminator);

/// Scans until non whitespace encounted (as described by `com_format_is_whitespace`)
/// REQUIRES: `reader` is a valid pointer to a valid com_reader
/// REQUIRES: `reader` must support com_reader_BUFFERED flag
/// GUARANTEES: the next read from `reader` will be a non whitespace character
/// GUARANTEES: on a read error will stop reading 
void com_scan_skip_whitespace(com_reader *reader);

#endif

