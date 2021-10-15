# Used to extract statistics from the test results.

import statistics

try:
    while True:
        filename = input("Enter the filename: ")
        #print("Reading file...")
        file = open(filename)
        lines = file.readlines()
        file.close()

        #print("Parsing...")

        intro_lines = []
        number_titles = []
        numbers = []

        test_counter = 0
        number_row = []
        line_counter = 0
        def get_line():
            line = lines[line_counter]
            if line[-1] == '\n':
                line = line[:-1]
            return line
        def is_test_title():
            return get_line() == "Test " + str(test_counter) + ":"
        while line_counter < len(lines) and not is_test_title():
            intro_lines.append(get_line())
            line_counter += 1
        while line_counter < len(lines):
            line = get_line()
            if is_test_title():
                if test_counter != 0:
                    numbers.append(number_row)
                    number_row = []
                test_counter += 1
            elif len(line) != 0 and ('0' <= line[-1] <= '9' or line[-1] == '.'):
                e = len(line)
                s = len(line) - 1
                while s > 0 and ('0' <= line[s - 1] <= '9' or line[s - 1] == '.'):
                    s -= 1
                if test_counter == 1:
                    number_titles.append(line[:s])
                number_row.append(float(line[s:e]))
            line_counter += 1
        if test_counter != 0:
            numbers.append(number_row)
            number_row = []
        #print ("Count: " + str(len(numbers)))

        if len(numbers) == 0:
            print("0 Count, skipping...")
            continue

        print()
        #print("Results for '" + filename + "':")
        #print()
        print("Population: " + str(len(numbers)))
        print()

        for line in intro_lines:
            print(line)
        for i in range(len(numbers[0])):
            population = []
            for row in numbers:
                population.append(row[i])
            indentation = ' ' * len(number_titles[i])
            print(number_titles[i] + "Mean: " + str(statistics.mean(population)))
            print(indentation + "Median: " + str(statistics.median(population)))
            print(indentation + "STDev: " + str(statistics.stdev(population)))
            print()
except KeyboardInterrupt:
    print()
