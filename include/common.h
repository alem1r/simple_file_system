#pragma once

#define CHECK_ERROR(cond, msg)      do {                                 \
                                        if (cond) {                      \
                                            fprintf(stderr, msg);        \
                                            exit(EXIT_FAILURE);          \
                                        }                                \
                                    } while(0)

#define DEBUG 0
