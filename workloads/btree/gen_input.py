import sys
import string
import random

input_size = 3000000

print "#include \"btree.h\"\n"

print "unsigned long input_table[] = {"


for i in range(input_size):
    key_val = random.randint(0, 18446744073709551615)
    if (i == (input_size -1)):
        print str(key_val) + "UL"
    else:
        print str(key_val) + "UL,"

print "};"
