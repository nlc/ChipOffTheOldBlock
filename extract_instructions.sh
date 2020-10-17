# # Pull them in and turn them into function exteriors
# awk '/^[^ ]/{printf("void c8_%s(Chip8 *c8, u16 instruction) {\n}\n", $1)}' instructions.txt

# # Pull them in and turn them into function calls
# awk '/^[^ ]/{printf("%s\nc8_%s(c8, instruction);\n", $1, $1)}' instructions.txt

# Pull them in and turn them into simple function names
awk '/^[^ ]/{printf("%s\n", $1, $1)}' instructions.txt
