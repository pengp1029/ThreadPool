set(SRC_LIST threadpool.cpp)
add_library(threadpool ${SRC_LIST})
target_link_libraries(threadpool pthread)