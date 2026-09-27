# the special fd "-" can be used to close files

ls .. 2>&1 1>&-
