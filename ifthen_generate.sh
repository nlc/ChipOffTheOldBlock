source instruction_to_test.sh && source extract_instructions.sh | while read str; do instruction_to_test "$str"; echo "  c8_$str(c8, instruction);"; done
