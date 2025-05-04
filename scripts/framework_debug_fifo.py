import os
import time
import select
import re
import threading
import subprocess

path = "/tmp/ospfw"


if os.path.exists(path):
    print(f"removing existing file {path}\n")
    os.remove(path)

os.mkfifo(path, mode=0o777)
print(f"FIFO created at {path}\n")


graph_regex = re.compile(r"<(NEW|UPDATE)_GRAPH>\n([0-9]+)ns\n(.*?)</(NEW|UPDATE)_GRAPH>", re.DOTALL)

buffer = ""
latest_graph_data = ""
exit_write_loop = False


def write_loop():
    global exit_write_loop
    global latest_graph_data
    while not exit_write_loop:
        if latest_graph_data:
            graph_data_write = latest_graph_data
            latest_graph_data = ""
            print("writing")

            p = subprocess.run(["dot", "-Gsize=16,9\\!", "-o", "/tmp/graph.png", "-Tpng"], input=graph_data_write, text=True)
        time.sleep(0.25)


write_loop_thread = threading.Thread(target=write_loop)
write_loop_thread.start()

try:
    while True:
        with open(path, 'r') as fifo:
            while True:
                #select.select([fifo], [], [fifo])
                buffer += fifo.readline()
                while True:
                    found = graph_regex.search(buffer)
                    if found:
                        print(f"Got graph: {found.group(1)} startTime={found.group(2)}" )
                        buffer = buffer[found.span()[1]:]
                        latest_graph_data = found.group(3)
                    else:
                        break

except KeyboardInterrupt:
    print("Exit. removing FIFO\n")
    os.remove(path)
    exit_write_loop = True
    write_loop_thread.join()

