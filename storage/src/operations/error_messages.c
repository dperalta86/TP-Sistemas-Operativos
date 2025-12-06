#include "error_messages.h"
#include "errors.h"

const char *storage_error_message(int code) {
    switch (code) {
        case 0:
            return "SUCCESS";
        case FILE_TAG_MISSING:
            return "FILE_TAG_MISSING";
        case FILE_TAG_ALREADY_EXISTS:
            return "FILE_TAG_ALREADY_EXISTS";
        case FILE_ALREADY_COMMITTED:
            return "FILE_ALREADY_COMMITTED";
        case READ_OUT_OF_BOUNDS:
            return "OUT_OF_BOUNDS";
        case NOT_ENOUGH_SPACE:
            return "NOT_ENOUGH_SPACE";
        default: {
            return "UNKNOWN_STORAGE_ERROR";
        }
    }
}
