#!/bin/sh

find . -regex '.*\.\(cpp\|hpp\)' -not -path "*build*" -exec clang-format -i {} \; 
