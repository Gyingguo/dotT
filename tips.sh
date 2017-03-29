# upload find files to hadoop
find . -type f -not -name '*_result' -print0 | xargs -0 -I {}  hadoop fs -put {} /path
