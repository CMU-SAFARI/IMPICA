import sys
import string
import random

input_size = 1572864
hash_size = 2 ** 20
#input_size = 10

print "#include \"hashtable.h\"\n"

print "input_item input_table[] = {"


for i in range(input_size):
    key_size = random.randint(20, 120)
    hash_value = random.randint(0, hash_size - 1)
    key_str = ''.join(random.choice(string.ascii_uppercase + \
                                string.ascii_lowercase + string.digits) for _ in range(key_size))
    print "{", str(key_size) + ", " + str(hash_value) + ", \"" + key_str + "\"" + "},"

print "};"
