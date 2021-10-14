# Used to convert times to positions in the figure.

try:
    start = float(input("Enter the start position: "))
    stop = float (input("Enter the stop position: "))

    length = stop - start

    start_time = float(input("Enter the (non-negative) start time: "))
    stop_time = float (input("Enter the (non-negative) stop time: "))

    time_interval = stop_time - start_time

    prev_t = 0
    while True:
        time = input("Enter a non-negative time or a time range (<num>-<num>) to convert: ")
        for t in time.split('-'):
            t = float(t)
            t = (t - start_time) / time_interval
            t = start + t * length
            print(str(t) + ", diff=" + str(t - prev_t))
            prev_t = t

except KeyboardInterrupt:
    print()
    exit()
