# Used to plot progressive test results.

"""
python plot-progressive-results.py parallel_progressive_test/test-2 "ParallelGroup in 2 threads with 2 modules vs 2 threads"
python plot-progressive-results.py parallel_progressive_test/test-4 "ParallelGroup in 4 threads with 4 modules vs 4 threads"
python plot-progressive-results.py parallel_progressive_test/test-8 "ParallelGroup in 8 threads with 8 modules vs 8 threads"
python plot-progressive-results.py parallel_progressive_test/test-80-0 "ParallelGroup with 80 modules vs 80 threads" "Threads time / LoopScheduler time"
python plot-progressive-results.py parallel_progressive_test/test-80-1 "ParallelGroup with 80 modules vs 8 threads running 80 modules"
python plot-progressive-results.py sequential_progressive_test/test-4-2 "SequentialGroup in 4 threads vs simple loop"
python plot-progressive-results.py sequential_progressive_test/test-1-2 "SequentialGroup in 1 thread vs simple loop"
"""

import io
import matplotlib.pyplot
import os
import pandas
import sys

efficiency_y_label = "Efficiency"

if len(sys.argv) == 3:
    filename = sys.argv[1]
    title = sys.argv[2]
elif len(sys.argv) == 4:
    filename = sys.argv[1]
    title = sys.argv[2]
    efficiency_y_label = sys.argv[3]
else:
    print("arguments: <filename> <title> [<efficiency_y_label>]")
    exit()

i = filename.rfind('/')
if i == -1:
    image_filename = "fig/" + filename
    if not os.path.exists("fig/"):
        os.makedirs("fig/")
else:
    image_filename = filename[:i] + "/fig" + filename[i:]
    if not os.path.exists(filename[:i] + "/fig"):
        os.makedirs(filename[:i] + "/fig")

file = open(filename)
text = file.read()
file.close()
text = text[text.find("work_amount,"):]
data = pandas.read_csv(io.StringIO(text))

data["efficiency_ma"] = data["efficiency"].rolling(window=10).mean().shift(-5)

t_s = data["avg_work_amount_time"][0]
t_e = data["avg_work_amount_time"][len(data["avg_work_amount_time"])-1]
t_d = (t_e - t_s) / (len(data["avg_work_amount_time"])-1)
data["processed_avg_work_amount_time"] = [t_s + i * t_d for i in range(len(data["avg_work_amount_time"]))]

matplotlib.pyplot.plot(data["processed_avg_work_amount_time"], data["efficiency"], '.')
matplotlib.pyplot.plot(data["processed_avg_work_amount_time"], data["efficiency_ma"], '-', label="Moving average")
matplotlib.pyplot.xlabel("Work amount average time in seconds")
matplotlib.pyplot.ylabel(efficiency_y_label)
matplotlib.pyplot.title(title)
matplotlib.pyplot.legend(loc="lower right")
matplotlib.pyplot.grid(True)
fig = matplotlib.pyplot.gcf()
matplotlib.pyplot.show()
fig.savefig(image_filename + "-a.png")

temp = data[data["loopscheduler_iterations_per_second"] <= 200]
if len(temp) == 0:
    temp = data[data["work_amount"] >= 2000]
data = temp

matplotlib.pyplot.plot(data["processed_avg_work_amount_time"], data["loopscheduler_iterations_per_second"], label="LoopScheduler", color='#f08', marker='.', linestyle="")
if "threads_iterations_per_second" in data:
    matplotlib.pyplot.plot(data["processed_avg_work_amount_time"], data["threads_iterations_per_second"], label="Threads", color='#0d8', marker='.', linestyle="")
elif "simple_loop_iterations_per_second" in data:
    matplotlib.pyplot.plot(data["processed_avg_work_amount_time"], data["simple_loop_iterations_per_second"], label="Simple loop", color='#0d8', marker='.', linestyle="")
matplotlib.pyplot.xlabel("Work amount average time (s)")
matplotlib.pyplot.ylabel("Iterations per second")
matplotlib.pyplot.title(title)
matplotlib.pyplot.legend(loc="upper right")
matplotlib.pyplot.grid(True)
fig = matplotlib.pyplot.gcf()
matplotlib.pyplot.show()
fig.savefig(image_filename + "-b.png")
