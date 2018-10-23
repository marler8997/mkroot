char *_concat(unsigned count,  ...);

#define CONCAT_ARG_11(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,...) a11
#define CONCAT_COUNT_ARGS(...) CONCAT_ARG_11(ignore, ##__VA_ARGS__,9,8,7,6,5,4,3,2,1,0)
#define concat(...) _concat(CONCAT_COUNT_ARGS(__VA_ARGS__), __VA_ARGS__)
