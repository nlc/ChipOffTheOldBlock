# c8_1nnn --> (instruction & 0xF000 == 0x1000)
instruction_to_test() {
  local instr="$(echo $1 | sed 's/^.*_//g')"

  # Replace numbers with F, non-numbers with 0
  local mask=$(echo "$instr" | sed -e 's/[0-9A-F]/F/g' -e 's/[a-z]/0/g')

  # Replace non-numbers with 0
  local predicate=$(echo "$instr" | sed -e 's/[a-z]/0/g')

  echo "} else if(instruction & 0x$mask == 0x$predicate) {"
}
