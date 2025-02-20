import os
import time
import random
import argparse
import subprocess
import threading


class Simulator:
    def __init__(self, container_name: str, mem_limit: int):
        self.container_name = container_name
        self.mem_limit = mem_limit
        self._io_count = 0
        self._restart_container()
        
        self.io_thread = threading.Thread(target=self._monitor_io_operations, daemon=True)
        self.io_thread.start()
        
        self.stress_thread = threading.Thread(target=self._run_stress_tests, daemon=True)
        self.stress_thread.start()

    def _run_stress_tests(self):
        try:
            while True:
                self.simulate_power_failure()
                time.sleep(random.randint(10, 30))
                self.simulate_memory_limit()
                time.sleep(random.randint(10, 30))
                self.simulate_disk_failure()
                time.sleep(random.randint(10, 30))
                self.simulate_network_delay()
                time.sleep(random.randint(10, 30))
                self.simulate_memory_errors()
                time.sleep(random.randint(10, 30))
        except KeyboardInterrupt:
            print("Stopped stress tests.")

    def _monitor_io_operations(self, directory: str = "/db"):
        command = f"docker exec {self.container_name} inotifywait -m -r -e open -e modify --format '%e %w%f' {directory}"
        try:
            process = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE)
            for line in process.stdout:
                event = line.decode('utf-8').strip()
                if "OPEN" in event or "MODIFY" in event:
                    self._io_count += 1
        except KeyboardInterrupt:
            print("Stopped monitoring IO operations.")

    def _flip_bit(self, byte_array: bytes, num_bits: int = 1):
        byte_list = bytearray(byte_array)
        for _ in range(num_bits):
            index = random.randint(0, len(byte_list) - 1)
            bit_position = random.randint(0, 7)
            byte_list[index] ^= (1 << bit_position)
        return bytes(byte_list)

    def _restart_container(self):
        os.system(f"docker start {self.container_name}")
        time.sleep(5)

    def simulate_power_failure(self):
        os.system(f"docker stop {self.container_name}")
        self._restart_container()

    def simulate_memory_limit(self):
        os.system(f"docker update --memory={self.mem_limit}k {self.container_name}")

    def simulate_disk_failure(self, delay: int = 5):
        os.system(f"docker exec {self.container_name} chmod 000 /db")
        time.sleep(delay)
        os.system(f"docker exec {self.container_name} chmod 755 /db")

    def simulate_network_delay(self, delay_ms: int = 5):
        os.system(f"docker network disconnect bridge {self.container_name}")
        time.sleep(delay_ms / 1000)
        os.system(f"docker network connect bridge {self.container_name}")

    def simulate_memory_errors(self, process_name: str = "cdbms_x86-64", interval: int = 5):
        while True:
            pid_command = f"docker exec {self.container_name} pgrep {process_name}"
            pid_result = os.popen(pid_command).read().strip()
            if not pid_result:
                print(f"Process '{process_name}' not found!")
                break

            pid = pid_result.split("\n")[0]
            mem_read_cmd = f"docker exec {self.container_name} cat /proc/{pid}/mem 2>/dev/null"
            mem_data = os.popen(mem_read_cmd).read()
            if not mem_data:
                print("Mem read error!")
                break

            corrupted_data = self._flip_bit(mem_data.encode())
            mem_write_cmd = f"docker exec {self.container_name} sh -c 'echo \"{corrupted_data.decode(errors='ignore')}\" > /proc/{pid}/mem'"
            os.system(mem_write_cmd)
            time.sleep(interval)


def _parse_args():
    parser = argparse.ArgumentParser(description="Simulation environment")
    parser.add_argument("--container", type=str, required=True, help="Container name")
    parser.add_argument("--mem-lim", type=int, default=100, help="Memory limit in KB")
    return parser.parse_args()


if __name__ == "__main__":
    args = _parse_args()
    Simulator(container_name=args.container, mem_limit=args.mem_lim)
    
    while True:
        time.sleep(1)
