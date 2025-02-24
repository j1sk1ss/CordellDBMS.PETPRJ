import re


log_file = "log.txt"

alloc_re = re.compile(r'\[MEM\] \[(.*?)\] \((.*?):(\d+)\) Allocated node \[(0x[0-9a-fA-F]+)\] with \[(\d+)\] size')
free_re = re.compile(r'\[MEM\] \[(.*?)\] \((.*?):(\d+)\) Free \[(0x[0-9a-fA-F]+)\]')

allocations = {}
leaks = {}


with open(log_file, "r", encoding="utf-8") as f:
    for line_number, line in enumerate(f, start=1):
        alloc_match = alloc_re.search(line)
        if alloc_match:
            timestamp, file, line_no, address, size = alloc_match.groups()
            allocations[address] = (int(size), file, line_no, line_number)
            leaks[address] = (int(size), file, line_no, line_number)
            continue
        
        free_match = free_re.search(line)
        if free_match:
            timestamp, file, line_no, address = free_match.groups()
            if address in leaks:
                del leaks[address]


if leaks:
    for addr, (size, file, line_no, log_line_no) in leaks.items():
        print(f"Addr: {addr}, Size: {size}, Allocated at: {file}:{line_no}, Log line: {log_line_no}")
else:
    print("Утечек памяти не найдено.")
