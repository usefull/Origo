---

Memory leaks detection
~~~~~~~~~~~~~~~~~~~~~~
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         ./build/debug/origo