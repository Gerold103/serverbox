**OS**: Debian GNU/Linux 11 (bullseye)<br>
**CPU**: AMD EPYC 7502P Processor 2.5GHz, 32 cores, 2 threads per core

---
**Nano load, 1 thread, 10 000 000 tasks, each is executed 1 time**

```
Command line args: -load nano -threads 1 -tasks 10000000 -exes 1
Run count: 5
Summary:
* 18 789 872 tasks per second;
* x5.099 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:      18 694 775
Exec per second median:   18 789 872
Exec per second max:      18 903 738

== Median report:
Microseconds per exec:          0.053220
Exec per second:                18789872
Mutex contention count:                0
Thread  0: exec:     10000000, sched:      2002


#### Trivial task scheduler
== Aggregated report:
Exec per second min:       1 877 993
Exec per second median:    3 684 654
Exec per second max:       6 059 699

== Median report:
Microseconds per exec:          0.271396
Exec per second:                 3684654
Mutex contention count:           199798
Thread  0: exec:     10000000, sched:         0

```
</details>

---
**Nano load, 1 thread, 5 000 000 tasks, each is executed 10 times**

```
Command line args: -load nano -threads 1 -tasks 5000000 -exes 10
Run count: 5
Summary:
* 22 720 409 tasks per second;
* x1.470 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:      21 475 118
Exec per second median:   22 720 409
Exec per second max:      22 801 487

== Median report:
Microseconds per exec:          0.044013
Exec per second:                22720409
Mutex contention count:                0
Thread  0: exec:     50000000, sched:     10003


#### Trivial task scheduler
== Aggregated report:
Exec per second min:      10 336 662
Exec per second median:   15 458 711
Exec per second max:      18 716 550

== Median report:
Microseconds per exec:          0.064688
Exec per second:                15458711
Mutex contention count:          1881745
Thread  0: exec:     50000000, sched:         0

```
</details>

---
**Nano load, 2 threads, 10 000 000 tasks, each is executed 1 time**

```
Command line args: -load nano -threads 2 -tasks 10000000 -exes 1
Run count: 5
Summary:
* 3 865 136 tasks per second;
* x1.291 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       3 740 973
Exec per second median:    3 865 136
Exec per second max:       5 212 684

== Median report:
Microseconds per exec:          0.517446
Exec per second:                 3865136
Exec per second per thread:      1932568
Mutex contention count:              730
Thread  0: exec:      5007394, sched:       513
Thread  1: exec:      4992606, sched:       501


#### Trivial task scheduler
== Aggregated report:
Exec per second min:       2 388 926
Exec per second median:    2 995 053
Exec per second max:       3 289 653

== Median report:
Microseconds per exec:          0.667768
Exec per second:                 2995053
Exec per second per thread:      1497526
Mutex contention count:          2409434
Thread  0: exec:      5118876, sched:         0
Thread  1: exec:      4881124, sched:         0

```
</details>

---
**Nano load, 2 threads, 5 000 000 tasks, each is executed 10 times**

```
Command line args: -load nano -threads 2 -tasks 5000000 -exes 10
Run count: 5
Summary:
* 4 824 827 tasks per second;
* x1.903 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       4 550 990
Exec per second median:    4 824 827
Exec per second max:       4 899 089

== Median report:
Microseconds per exec:          0.414523
Exec per second:                 4824827
Exec per second per thread:      2412413
Mutex contention count:             4085
Thread  0: exec:     25059728, sched:      2512
Thread  1: exec:     24940272, sched:      2543


#### Trivial task scheduler
== Aggregated report:
Exec per second min:       1 786 527
Exec per second median:    2 535 079
Exec per second max:       2 594 717

== Median report:
Microseconds per exec:          0.788930
Exec per second:                 2535079
Exec per second per thread:      1267539
Mutex contention count:         13354463
Thread  0: exec:     25985942, sched:         0
Thread  1: exec:     24014058, sched:         0

```
</details>

---
**Nano load, 3 threads, 10 000 000 tasks, each is executed 1 time**

```
Command line args: -load nano -threads 3 -tasks 10000000 -exes 1
Run count: 5
Summary:
* 2 600 415 tasks per second;
* x1.625 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       2 570 247
Exec per second median:    2 600 415
Exec per second max:       2 952 683

== Median report:
Microseconds per exec:          1.153662
Exec per second:                 2600415
Exec per second per thread:       866805
Mutex contention count:             3060
Thread  0: exec:      3387963, sched:       615
Thread  1: exec:      3227172, sched:       576
Thread  2: exec:      3384865, sched:       619


#### Trivial task scheduler
== Aggregated report:
Exec per second min:       1 474 664
Exec per second median:    1 600 148
Exec per second max:       1 737 105

== Median report:
Microseconds per exec:          1.874826
Exec per second:                 1600148
Exec per second per thread:       533382
Mutex contention count:          4071644
Thread  0: exec:      3239134, sched:         0
Thread  1: exec:      3323945, sched:         0
Thread  2: exec:      3436921, sched:         0

```
</details>

---
**Nano load, 3 threads, 5 000 000 tasks, each is executed 10 times**

```
Command line args: -load nano -threads 3 -tasks 5000000 -exes 10
Run count: 5
Summary:
* 2 353 739 tasks per second;
* x1.752 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       2 261 854
Exec per second median:    2 353 739
Exec per second max:       2 615 564

== Median report:
Microseconds per exec:          1.274567
Exec per second:                 2353739
Exec per second per thread:       784579
Mutex contention count:            13605
Thread  0: exec:     16695534, sched:      1576
Thread  1: exec:     16636981, sched:      1565
Thread  2: exec:     16667485, sched:      1560


#### Trivial task scheduler
== Aggregated report:
Exec per second min:       1 324 463
Exec per second median:    1 343 749
Exec per second max:       1 398 340

== Median report:
Microseconds per exec:          2.232560
Exec per second:                 1343749
Exec per second per thread:       447916
Mutex contention count:         27193620
Thread  0: exec:     16729437, sched:         0
Thread  1: exec:     16813838, sched:         0
Thread  2: exec:     16456725, sched:         0

```
</details>

---
**Nano load, 5 threads, 10 000 000 tasks, each is executed 1 time**

```
Command line args: -load nano -threads 5 -tasks 10000000 -exes 1
Run count: 5
Summary:
* 2 816 844 tasks per second;
* x2.721 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       2 760 192
Exec per second median:    2 816 844
Exec per second max:       3 367 636

== Median report:
Microseconds per exec:          1.775036
Exec per second:                 2816844
Exec per second per thread:       563368
Mutex contention count:             6630
Thread  0: exec:      2020720, sched:       312
Thread  1: exec:      1993676, sched:       313
Thread  2: exec:      2017749, sched:       298
Thread  3: exec:      1973694, sched:       317
Thread  4: exec:      1994161, sched:       316


#### Trivial task scheduler
== Aggregated report:
Exec per second min:         947 287
Exec per second median:    1 035 286
Exec per second max:       1 081 781

== Median report:
Microseconds per exec:          4.829583
Exec per second:                 1035286
Exec per second per thread:       207057
Mutex contention count:          4872085
Thread  0: exec:      1975483, sched:         0
Thread  1: exec:      2011046, sched:         0
Thread  2: exec:      1966560, sched:         0
Thread  3: exec:      1991264, sched:         0
Thread  4: exec:      2055647, sched:         0

```
</details>

---
**Nano load, 5 threads, 5 000 000 tasks, each is executed 10 times**

```
Command line args: -load nano -threads 5 -tasks 5000000 -exes 10
Run count: 5
Summary:
* 2 761 703 tasks per second;
* x2.872 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       2 698 629
Exec per second median:    2 761 703
Exec per second max:       2 893 089

== Median report:
Microseconds per exec:          1.810477
Exec per second:                 2761703
Exec per second per thread:       552340
Mutex contention count:            31680
Thread  0: exec:     10047172, sched:       710
Thread  1: exec:     10040485, sched:       743
Thread  2: exec:      9828159, sched:       994
Thread  3: exec:     10032590, sched:       812
Thread  4: exec:     10051594, sched:       729


#### Trivial task scheduler
== Aggregated report:
Exec per second min:         754 657
Exec per second median:      961 482
Exec per second max:         995 214

== Median report:
Microseconds per exec:          5.200301
Exec per second:                  961482
Exec per second per thread:       192296
Mutex contention count:         35747826
Thread  0: exec:      9905804, sched:         0
Thread  1: exec:      9750017, sched:         0
Thread  2: exec:     10252264, sched:         0
Thread  3: exec:      9925992, sched:         0
Thread  4: exec:     10165923, sched:         0

```
</details>

---
**Nano load, 10 threads, 10 000 000 tasks, each is executed 1 time**

```
Command line args: -load nano -threads 10 -tasks 10000000 -exes 1
Run count: 5
Summary:
* 4 358 987 tasks per second;
* x7.794 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       4 308 048
Exec per second median:    4 358 987
Exec per second max:       4 767 636

== Median report:
Microseconds per exec:          2.294111
Exec per second:                 4358987
Exec per second per thread:       435898
Mutex contention count:            16363
Thread  0: exec:      1061472, sched:       184
Thread  1: exec:       972837, sched:       175
Thread  2: exec:      1061814, sched:       191
Thread  3: exec:       937443, sched:       169
Thread  4: exec:       939432, sched:       164
Thread  5: exec:       979234, sched:       167
Thread  6: exec:       948437, sched:       171
Thread  7: exec:      1059874, sched:       182
Thread  8: exec:       974526, sched:       169
Thread  9: exec:      1064931, sched:       191


#### Trivial task scheduler
== Aggregated report:
Exec per second min:         545 010
Exec per second median:      559 297
Exec per second max:         598 627

== Median report:
Microseconds per exec:         17.879585
Exec per second:                  559297
Exec per second per thread:        55929
Mutex contention count:          5112073
Thread  0: exec:       990237, sched:         0
Thread  1: exec:       985199, sched:         0
Thread  2: exec:       974498, sched:         0
Thread  3: exec:       973898, sched:         0
Thread  4: exec:       987826, sched:         0
Thread  5: exec:       966851, sched:         0
Thread  6: exec:       982061, sched:         0
Thread  7: exec:       969940, sched:         0
Thread  8: exec:      1174105, sched:         0
Thread  9: exec:       995385, sched:         0

```
</details>

---
**Nano load, 10 threads, 5 000 000 tasks, each is executed 10 times**

```
Command line args: -load nano -threads 10 -tasks 5000000 -exes 10
Run count: 5
Summary:
* 3 776 866 tasks per second;
* x4.117 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       3 546 717
Exec per second median:    3 776 866
Exec per second max:       3 823 845

== Median report:
Microseconds per exec:          2.647697
Exec per second:                 3776866
Exec per second per thread:       377686
Mutex contention count:            79997
Thread  0: exec:      4887070, sched:       501
Thread  1: exec:      5115581, sched:       473
Thread  2: exec:      5116095, sched:       475
Thread  3: exec:      4971561, sched:       482
Thread  4: exec:      4906911, sched:       468
Thread  5: exec:      4906383, sched:       472
Thread  6: exec:      4867906, sched:       541
Thread  7: exec:      5130560, sched:       414
Thread  8: exec:      4977653, sched:       460
Thread  9: exec:      5120280, sched:       443


#### Trivial task scheduler
== Aggregated report:
Exec per second min:         789 758
Exec per second median:      917 306
Exec per second max:         938 270

== Median report:
Microseconds per exec:         10.901479
Exec per second:                  917306
Exec per second per thread:        91730
Mutex contention count:         35655326
Thread  0: exec:      5063405, sched:         0
Thread  1: exec:      4977867, sched:         0
Thread  2: exec:      5024621, sched:         0
Thread  3: exec:      5031524, sched:         0
Thread  4: exec:      4933343, sched:         0
Thread  5: exec:      4847788, sched:         0
Thread  6: exec:      5075848, sched:         0
Thread  7: exec:      5046914, sched:         0
Thread  8: exec:      5068194, sched:         0
Thread  9: exec:      4930496, sched:         0

```
</details>

---
**Nano load, 50 threads, each task is executed 1 time**

```
Command line args: -load nano -threads 50 -exes 1
Run count: 5
Summary:
* 8 260 229 tasks per second;
* x168.525 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       7 975 123
Exec per second median:    8 260 229
Exec per second max:       8 307 552

== Median report:
Microseconds per exec:          6.053100
Exec per second:                 8260229
Exec per second per thread:       165204
Mutex contention count:           481481
Thread  0: exec:      1093200, sched:       177
Thread  1: exec:       948668, sched:       148
Thread  2: exec:      1095154, sched:       179
Thread  3: exec:      1054197, sched:       155
Thread  4: exec:       868788, sched:       135
Thread  5: exec:       957711, sched:       150
Thread  6: exec:      1097325, sched:       168
Thread  7: exec:       867508, sched:       136
Thread  8: exec:       875414, sched:       135
Thread  9: exec:       844463, sched:       133
Thread 10: exec:       885759, sched:       131
Thread 11: exec:      1389091, sched:       226
Thread 12: exec:       944304, sched:       143
Thread 13: exec:       868651, sched:       134
Thread 14: exec:       867620, sched:       131
Thread 15: exec:       990717, sched:       150
Thread 16: exec:      1090603, sched:       179
Thread 17: exec:      1068232, sched:       169
Thread 18: exec:      1096947, sched:       175
Thread 19: exec:      1066259, sched:       171
Thread 20: exec:      1075489, sched:       163
Thread 21: exec:       957468, sched:       146
Thread 22: exec:      1064200, sched:       165
Thread 23: exec:      1044650, sched:       151
Thread 24: exec:       955646, sched:       150
Thread 25: exec:      1054256, sched:       173
Thread 26: exec:      1087985, sched:       180
Thread 27: exec:       983992, sched:       146
Thread 28: exec:      1051633, sched:       165
Thread 29: exec:      1027313, sched:       174
Thread 30: exec:      1041316, sched:       164
Thread 31: exec:      1087739, sched:       178
Thread 32: exec:       905753, sched:       134
Thread 33: exec:      1079037, sched:       170
Thread 34: exec:       921735, sched:       140
Thread 35: exec:       860972, sched:       131
Thread 36: exec:       845929, sched:       134
Thread 37: exec:       950371, sched:       158
Thread 38: exec:       876948, sched:       137
Thread 39: exec:      1090315, sched:       176
Thread 40: exec:       902224, sched:       135
Thread 41: exec:      1105521, sched:       183
Thread 42: exec:      1031561, sched:       172
Thread 43: exec:       886065, sched:       130
Thread 44: exec:      1333432, sched:       217
Thread 45: exec:      1060997, sched:       172
Thread 46: exec:      1051607, sched:       172
Thread 47: exec:       888451, sched:       141
Thread 48: exec:       898766, sched:       135
Thread 49: exec:       908018, sched:       136


#### Trivial task scheduler
== Aggregated report:
Exec per second min:          43 282
Exec per second median:       49 015
Exec per second max:          93 692

== Median report:
Microseconds per exec:       1020.087692
Exec per second:                   49015
Exec per second per thread:          980
Mutex contention count:           666543
Thread  0: exec:        19349, sched:         0
Thread  1: exec:        19383, sched:         0
Thread  2: exec:        19872, sched:         0
Thread  3: exec:        19642, sched:         0
Thread  4: exec:        19481, sched:         0
Thread  5: exec:        21110, sched:         0
Thread  6: exec:        21625, sched:         0
Thread  7: exec:        19782, sched:         0
Thread  8: exec:        20685, sched:         0
Thread  9: exec:        19074, sched:         0
Thread 10: exec:        18804, sched:         0
Thread 11: exec:        18952, sched:         0
Thread 12: exec:        21385, sched:         0
Thread 13: exec:        18804, sched:         0
Thread 14: exec:        21305, sched:         0
Thread 15: exec:        20673, sched:         0
Thread 16: exec:        19203, sched:         0
Thread 17: exec:        19321, sched:         0
Thread 18: exec:        21827, sched:         0
Thread 19: exec:        19285, sched:         0
Thread 20: exec:        19514, sched:         0
Thread 21: exec:        19454, sched:         0
Thread 22: exec:        20689, sched:         0
Thread 23: exec:        19806, sched:         0
Thread 24: exec:        19398, sched:         0
Thread 25: exec:        19382, sched:         0
Thread 26: exec:        20788, sched:         0
Thread 27: exec:        20190, sched:         0
Thread 28: exec:        19080, sched:         0
Thread 29: exec:        20049, sched:         0
Thread 30: exec:        19222, sched:         0
Thread 31: exec:        19989, sched:         0
Thread 32: exec:        18799, sched:         0
Thread 33: exec:        20043, sched:         0
Thread 34: exec:        19584, sched:         0
Thread 35: exec:        21845, sched:         0
Thread 36: exec:        19988, sched:         0
Thread 37: exec:        20137, sched:         0
Thread 38: exec:        22558, sched:         0
Thread 39: exec:        19866, sched:         0
Thread 40: exec:        21112, sched:         0
Thread 41: exec:        19455, sched:         0
Thread 42: exec:        18839, sched:         0
Thread 43: exec:        19321, sched:         0
Thread 44: exec:        20154, sched:         0
Thread 45: exec:        19743, sched:         0
Thread 46: exec:        20127, sched:         0
Thread 47: exec:        20690, sched:         0
Thread 48: exec:        19696, sched:         0
Thread 49: exec:        20920, sched:         0

```
</details>

---
**Nano load, 50 threads, each task is executed 5 times**

```
Command line args: -load nano -threads 50 -exes 5
Run count: 5
Summary:
* 8 135 930 tasks per second;
* x19.771 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       4 545 022
Exec per second median:    8 135 930
Exec per second max:       8 459 053

== Median report:
Microseconds per exec:          6.145578
Exec per second:                 8135930
Exec per second per thread:       162718
Mutex contention count:           476241
Thread  0: exec:      1005687, sched:        88
Thread  1: exec:      1083295, sched:        90
Thread  2: exec:       990646, sched:        86
Thread  3: exec:       994105, sched:        70
Thread  4: exec:       948298, sched:        84
Thread  5: exec:      1279852, sched:        97
Thread  6: exec:       974077, sched:        89
Thread  7: exec:      1069700, sched:        70
Thread  8: exec:       945470, sched:        74
Thread  9: exec:       931551, sched:        74
Thread 10: exec:       942954, sched:        84
Thread 11: exec:       948783, sched:        65
Thread 12: exec:       888226, sched:        74
Thread 13: exec:      1080898, sched:        85
Thread 14: exec:      1163589, sched:       107
Thread 15: exec:       930980, sched:        75
Thread 16: exec:       828662, sched:        56
Thread 17: exec:      1029973, sched:        88
Thread 18: exec:       929399, sched:        72
Thread 19: exec:      1103761, sched:        81
Thread 20: exec:       985495, sched:        76
Thread 21: exec:       925424, sched:        78
Thread 22: exec:      1122857, sched:        99
Thread 23: exec:       947381, sched:        63
Thread 24: exec:       928609, sched:        70
Thread 25: exec:      1116043, sched:        87
Thread 26: exec:      1308477, sched:        84
Thread 27: exec:       924504, sched:        69
Thread 28: exec:       883062, sched:        72
Thread 29: exec:       936139, sched:        76
Thread 30: exec:       928680, sched:        76
Thread 31: exec:       890243, sched:        62
Thread 32: exec:       882099, sched:        76
Thread 33: exec:      1046153, sched:        72
Thread 34: exec:      1043538, sched:        81
Thread 35: exec:       934316, sched:        61
Thread 36: exec:       934964, sched:        76
Thread 37: exec:       945648, sched:        67
Thread 38: exec:       929724, sched:        67
Thread 39: exec:      1199942, sched:        98
Thread 40: exec:       928593, sched:        67
Thread 41: exec:      1047131, sched:        78
Thread 42: exec:      1006456, sched:        82
Thread 43: exec:       925147, sched:        71
Thread 44: exec:       915479, sched:        75
Thread 45: exec:       942788, sched:        60
Thread 46: exec:      1078789, sched:        85
Thread 47: exec:      1077035, sched:        80
Thread 48: exec:      1051985, sched:        75
Thread 49: exec:      1143393, sched:        91


#### Trivial task scheduler
== Aggregated report:
Exec per second min:         397 046
Exec per second median:      411 515
Exec per second max:         451 368

== Median report:
Microseconds per exec:        121.502201
Exec per second:                  411515
Exec per second per thread:         8230
Mutex contention count:          3501595
Thread  0: exec:        98148, sched:         0
Thread  1: exec:       100187, sched:         0
Thread  2: exec:       103386, sched:         0
Thread  3: exec:       101009, sched:         0
Thread  4: exec:       101038, sched:         0
Thread  5: exec:       100485, sched:         0
Thread  6: exec:        99480, sched:         0
Thread  7: exec:       100117, sched:         0
Thread  8: exec:       100739, sched:         0
Thread  9: exec:       102203, sched:         0
Thread 10: exec:        99719, sched:         0
Thread 11: exec:        98859, sched:         0
Thread 12: exec:        99874, sched:         0
Thread 13: exec:       100554, sched:         0
Thread 14: exec:        98976, sched:         0
Thread 15: exec:        98800, sched:         0
Thread 16: exec:       102780, sched:         0
Thread 17: exec:        98655, sched:         0
Thread 18: exec:        97891, sched:         0
Thread 19: exec:        97623, sched:         0
Thread 20: exec:       100686, sched:         0
Thread 21: exec:        99927, sched:         0
Thread 22: exec:       100693, sched:         0
Thread 23: exec:       100258, sched:         0
Thread 24: exec:       100275, sched:         0
Thread 25: exec:        98555, sched:         0
Thread 26: exec:       100397, sched:         0
Thread 27: exec:       102905, sched:         0
Thread 28: exec:        98391, sched:         0
Thread 29: exec:        97316, sched:         0
Thread 30: exec:        98746, sched:         0
Thread 31: exec:       100679, sched:         0
Thread 32: exec:        99857, sched:         0
Thread 33: exec:       100602, sched:         0
Thread 34: exec:        98912, sched:         0
Thread 35: exec:        98103, sched:         0
Thread 36: exec:        97496, sched:         0
Thread 37: exec:        98453, sched:         0
Thread 38: exec:        99428, sched:         0
Thread 39: exec:       101157, sched:         0
Thread 40: exec:       100513, sched:         0
Thread 41: exec:       101368, sched:         0
Thread 42: exec:       101003, sched:         0
Thread 43: exec:       102948, sched:         0
Thread 44: exec:       102355, sched:         0
Thread 45: exec:        97939, sched:         0
Thread 46: exec:        99758, sched:         0
Thread 47: exec:       102278, sched:         0
Thread 48: exec:        98350, sched:         0
Thread 49: exec:       100129, sched:         0

```
</details>

---
**Micro load, 1 thread, 1 000 000 tasks, each is executed 1 time**

```
Command line args: -load micro -threads 1 -tasks 1000000 -exes 1
Run count: 5
Summary:
* 550 638 tasks per second;
* almost same as trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:         550 216
Exec per second median:      550 638
Exec per second max:         552 093

== Median report:
Microseconds per exec:          1.816072
Exec per second:                  550638
Mutex contention count:                0
Thread  0: exec:      1000000, sched:       201


#### Trivial task scheduler
== Aggregated report:
Exec per second min:         549 092
Exec per second median:      554 056
Exec per second max:         556 045

== Median report:
Microseconds per exec:          1.804871
Exec per second:                  554056
Mutex contention count:             9385
Thread  0: exec:      1000000, sched:         0

```
</details>

---
**Micro load, 1 thread, 100 000 tasks, each is executed 10 times**

```
Command line args: -load micro -threads 1 -tasks 100000 -exes 10
Run count: 5
Summary:
* 553 803 tasks per second;
* almost same as trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:         553 112
Exec per second median:      553 803
Exec per second max:         555 129

== Median report:
Microseconds per exec:          1.805693
Exec per second:                  553803
Mutex contention count:                0
Thread  0: exec:      1000000, sched:       201


#### Trivial task scheduler
== Aggregated report:
Exec per second min:         552 446
Exec per second median:      552 980
Exec per second max:         554 718

== Median report:
Microseconds per exec:          1.808382
Exec per second:                  552980
Mutex contention count:             1803
Thread  0: exec:      1000000, sched:         0

```
</details>

---
**Micro load, 2 threads, 2 000 000 tasks, each is executed 1 time**

```
Command line args: -load micro -threads 2 -tasks 2000000 -exes 1
Run count: 5
Summary:
* 923 753 tasks per second;
* almost same as trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:         918 003
Exec per second median:      923 753
Exec per second max:         945 835

== Median report:
Microseconds per exec:          2.165079
Exec per second:                  923753
Exec per second per thread:       461876
Mutex contention count:               32
Thread  0: exec:      1000597, sched:       200
Thread  1: exec:       999403, sched:       201


#### Trivial task scheduler
== Aggregated report:
Exec per second min:         876 647
Exec per second median:      919 471
Exec per second max:         925 951

== Median report:
Microseconds per exec:          2.175163
Exec per second:                  919471
Exec per second per thread:       459735
Mutex contention count:            72255
Thread  0: exec:      1000079, sched:         0
Thread  1: exec:       999921, sched:         0

```
</details>

---
**Micro load, 2 threads, 200 000 tasks, each is executed 10 times**

```
Command line args: -load micro -threads 2 -tasks 200000 -exes 10
Run count: 5
Summary:
* 918 091 tasks per second;
* x1.031 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:         905 039
Exec per second median:      918 091
Exec per second max:         949 059

== Median report:
Microseconds per exec:          2.178432
Exec per second:                  918091
Exec per second per thread:       459045
Mutex contention count:               15
Thread  0: exec:       999298, sched:       124
Thread  1: exec:      1000702, sched:       127


#### Trivial task scheduler
== Aggregated report:
Exec per second min:         833 763
Exec per second median:      890 186
Exec per second max:         903 889

== Median report:
Microseconds per exec:          2.246720
Exec per second:                  890186
Exec per second per thread:       445093
Mutex contention count:            86293
Thread  0: exec:       999656, sched:         0
Thread  1: exec:      1000344, sched:         0

```
</details>

---
**Micro load, 3 threads, 3 000 000 tasks, each is executed 1 time**

```
Command line args: -load micro -threads 3 -tasks 3000000 -exes 1
Run count: 5
Summary:
* 1 302 571 tasks per second;
* x1.087 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       1 292 218
Exec per second median:    1 302 571
Exec per second max:       1 319 149

== Median report:
Microseconds per exec:          2.303136
Exec per second:                 1302571
Exec per second per thread:       434190
Mutex contention count:               90
Thread  0: exec:      1001219, sched:       200
Thread  1: exec:      1000569, sched:       199
Thread  2: exec:       998212, sched:       199


#### Trivial task scheduler
== Aggregated report:
Exec per second min:       1 169 866
Exec per second median:    1 198 129
Exec per second max:       1 230 044

== Median report:
Microseconds per exec:          2.503903
Exec per second:                 1198129
Exec per second per thread:       399376
Mutex contention count:           509332
Thread  0: exec:       998915, sched:         0
Thread  1: exec:       998786, sched:         0
Thread  2: exec:      1002299, sched:         0

```
</details>

---
**Micro load, 3 threads, 300 000 tasks, each is executed 10 times**

```
Command line args: -load micro -threads 3 -tasks 300000 -exes 10
Run count: 5
Summary:
* 1 216 992 tasks per second;
* x1.160 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       1 209 464
Exec per second median:    1 216 992
Exec per second max:       1 234 562

== Median report:
Microseconds per exec:          2.465093
Exec per second:                 1216992
Exec per second per thread:       405664
Mutex contention count:              129
Thread  0: exec:      1001499, sched:        94
Thread  1: exec:       996307, sched:       112
Thread  2: exec:      1002194, sched:        99


#### Trivial task scheduler
== Aggregated report:
Exec per second min:         988 258
Exec per second median:    1 048 852
Exec per second max:       1 109 512

== Median report:
Microseconds per exec:          2.860269
Exec per second:                 1048852
Exec per second per thread:       349617
Mutex contention count:           761162
Thread  0: exec:       994089, sched:         0
Thread  1: exec:      1003581, sched:         0
Thread  2: exec:      1002330, sched:         0

```
</details>

---
**Micro load, 5 threads, 5 000 000 tasks, each is executed 1 time**

```
Command line args: -load micro -threads 5 -tasks 5000000 -exes 1
Run count: 5
Summary:
* 1 895 964 tasks per second;
* x1.822 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       1 876 132
Exec per second median:    1 895 964
Exec per second max:       1 943 526

== Median report:
Microseconds per exec:          2.637180
Exec per second:                 1895964
Exec per second per thread:       379192
Mutex contention count:             1385
Thread  0: exec:      1003122, sched:       196
Thread  1: exec:       997256, sched:       192
Thread  2: exec:       997653, sched:       192
Thread  3: exec:      1002901, sched:       197
Thread  4: exec:       999068, sched:       197


#### Trivial task scheduler
== Aggregated report:
Exec per second min:       1 003 157
Exec per second median:    1 040 378
Exec per second max:       1 169 196

== Median report:
Microseconds per exec:          4.805942
Exec per second:                 1040378
Exec per second per thread:       208075
Mutex contention count:          2534455
Thread  0: exec:       985615, sched:         0
Thread  1: exec:      1007408, sched:         0
Thread  2: exec:       990643, sched:         0
Thread  3: exec:      1004154, sched:         0
Thread  4: exec:      1012180, sched:         0

```
</details>

---
**Micro load, 5 threads, 500 000 tasks, each is executed 10 times**

```
Command line args: -load micro -threads 5 -tasks 500000 -exes 10
Run count: 5
Summary:
* 1 796 108 tasks per second;
* x2.238 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       1 765 175
Exec per second median:    1 796 108
Exec per second max:       1 921 756

== Median report:
Microseconds per exec:          2.783796
Exec per second:                 1796108
Exec per second per thread:       359221
Mutex contention count:             1402
Thread  0: exec:      1004084, sched:        74
Thread  1: exec:       998733, sched:        99
Thread  2: exec:      1004362, sched:        75
Thread  3: exec:       998908, sched:        94
Thread  4: exec:       993913, sched:       121


#### Trivial task scheduler
== Aggregated report:
Exec per second min:         785 226
Exec per second median:      802 610
Exec per second max:         969 324

== Median report:
Microseconds per exec:          6.229672
Exec per second:                  802610
Exec per second per thread:       160522
Mutex contention count:          3737219
Thread  0: exec:       991169, sched:         0
Thread  1: exec:      1015385, sched:         0
Thread  2: exec:       980582, sched:         0
Thread  3: exec:      1015217, sched:         0
Thread  4: exec:       997647, sched:         0

```
</details>

---
**Micro load, 10 threads, 10 000 000 tasks, each is executed 1 time**

```
Command line args: -load micro -threads 10 -tasks 10000000 -exes 1
Run count: 5
Summary:
* 2 899 057 tasks per second;
* x4.099 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       2 871 082
Exec per second median:    2 899 057
Exec per second max:       3 255 779

== Median report:
Microseconds per exec:          3.449397
Exec per second:                 2899057
Exec per second per thread:       289905
Mutex contention count:            12875
Thread  0: exec:       981508, sched:       190
Thread  1: exec:       984217, sched:       183
Thread  2: exec:       993185, sched:       180
Thread  3: exec:      1024771, sched:       196
Thread  4: exec:      1021472, sched:       194
Thread  5: exec:       976119, sched:       181
Thread  6: exec:       978350, sched:       184
Thread  7: exec:       995508, sched:       188
Thread  8: exec:      1022789, sched:       191
Thread  9: exec:      1022081, sched:       187


#### Trivial task scheduler
== Aggregated report:
Exec per second min:         649 222
Exec per second median:      707 310
Exec per second max:         726 180

== Median report:
Microseconds per exec:         14.138057
Exec per second:                  707310
Exec per second per thread:        70731
Mutex contention count:          6876372
Thread  0: exec:      1001244, sched:         0
Thread  1: exec:      1002832, sched:         0
Thread  2: exec:      1000778, sched:         0
Thread  3: exec:      1004727, sched:         0
Thread  4: exec:       998702, sched:         0
Thread  5: exec:       995675, sched:         0
Thread  6: exec:       998708, sched:         0
Thread  7: exec:       998443, sched:         0
Thread  8: exec:      1000493, sched:         0
Thread  9: exec:       998398, sched:         0

```
</details>

---
**Micro load, 10 threads, 1 000 000 tasks, each is executed 10 times**

```
Command line args: -load micro -threads 10 -tasks 1000000 -exes 10
Run count: 5
Summary:
* 2 964 800 tasks per second;
* x3.833 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       2 894 485
Exec per second median:    2 964 800
Exec per second max:       2 995 940

== Median report:
Microseconds per exec:          3.372908
Exec per second:                 2964800
Exec per second per thread:       296480
Mutex contention count:            13698
Thread  0: exec:       997175, sched:        98
Thread  1: exec:      1009882, sched:        71
Thread  2: exec:      1009468, sched:        85
Thread  3: exec:       992541, sched:        90
Thread  4: exec:       992103, sched:       101
Thread  5: exec:       996907, sched:        85
Thread  6: exec:      1003012, sched:        80
Thread  7: exec:       996491, sched:        96
Thread  8: exec:      1003768, sched:        82
Thread  9: exec:       998653, sched:        88


#### Trivial task scheduler
== Aggregated report:
Exec per second min:         678 796
Exec per second median:      773 445
Exec per second max:         822 276

== Median report:
Microseconds per exec:         12.929158
Exec per second:                  773445
Exec per second per thread:        77344
Mutex contention count:          8655485
Thread  0: exec:      1005160, sched:         0
Thread  1: exec:       990185, sched:         0
Thread  2: exec:       973703, sched:         0
Thread  3: exec:      1011513, sched:         0
Thread  4: exec:      1017217, sched:         0
Thread  5: exec:      1020006, sched:         0
Thread  6: exec:       984039, sched:         0
Thread  7: exec:       991515, sched:         0
Thread  8: exec:       999399, sched:         0
Thread  9: exec:      1007265, sched:         0

```
</details>

---
**Micro load, 50 threads, each task is executed 1 time**

```
Command line args: -load micro -threads 50 -exes 1
Run count: 5
Summary:
* 7 507 201 tasks per second;
* x118.836 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       7 022 035
Exec per second median:    7 507 201
Exec per second max:       7 961 451

== Median report:
Microseconds per exec:          6.660272
Exec per second:                 7507201
Exec per second per thread:       150144
Mutex contention count:           468074
Thread  0: exec:       936408, sched:       154
Thread  1: exec:       929207, sched:       150
Thread  2: exec:       970888, sched:       168
Thread  3: exec:       967628, sched:       167
Thread  4: exec:       942739, sched:       149
Thread  5: exec:      1016424, sched:       168
Thread  6: exec:      1151763, sched:       185
Thread  7: exec:      1004273, sched:       156
Thread  8: exec:      1062346, sched:       167
Thread  9: exec:       937169, sched:       158
Thread 10: exec:       954562, sched:       163
Thread 11: exec:       926951, sched:       159
Thread 12: exec:       968878, sched:       153
Thread 13: exec:       948199, sched:       155
Thread 14: exec:       974409, sched:       155
Thread 15: exec:      1000342, sched:       154
Thread 16: exec:      1057965, sched:       165
Thread 17: exec:      1110656, sched:       180
Thread 18: exec:      1025146, sched:       172
Thread 19: exec:       946357, sched:       168
Thread 20: exec:      1063432, sched:       167
Thread 21: exec:       938865, sched:       152
Thread 22: exec:       938954, sched:       148
Thread 23: exec:      1043815, sched:       174
Thread 24: exec:      1035557, sched:       177
Thread 25: exec:       976444, sched:       158
Thread 26: exec:      1049544, sched:       172
Thread 27: exec:       986657, sched:       148
Thread 28: exec:       958532, sched:       161
Thread 29: exec:       942232, sched:       163
Thread 30: exec:      1080032, sched:       174
Thread 31: exec:       967938, sched:       167
Thread 32: exec:      1007436, sched:       164
Thread 33: exec:       933556, sched:       163
Thread 34: exec:       965041, sched:       170
Thread 35: exec:       934191, sched:       150
Thread 36: exec:      1043295, sched:       174
Thread 37: exec:      1036009, sched:       168
Thread 38: exec:       977168, sched:       157
Thread 39: exec:      1159464, sched:       188
Thread 40: exec:      1032539, sched:       161
Thread 41: exec:       924083, sched:       155
Thread 42: exec:      1112503, sched:       183
Thread 43: exec:      1063001, sched:       161
Thread 44: exec:       958291, sched:       158
Thread 45: exec:       985755, sched:       154
Thread 46: exec:      1049566, sched:       169
Thread 47: exec:       952707, sched:       151
Thread 48: exec:      1064264, sched:       164
Thread 49: exec:       986819, sched:       151


#### Trivial task scheduler
== Aggregated report:
Exec per second min:          56 974
Exec per second median:       63 173
Exec per second max:          67 833

== Median report:
Microseconds per exec:        791.469743
Exec per second:                   63173
Exec per second per thread:         1263
Mutex contention count:           797461
Thread  0: exec:        19906, sched:         0
Thread  1: exec:        19542, sched:         0
Thread  2: exec:        20171, sched:         0
Thread  3: exec:        19744, sched:         0
Thread  4: exec:        19762, sched:         0
Thread  5: exec:        19995, sched:         0
Thread  6: exec:        20102, sched:         0
Thread  7: exec:        19661, sched:         0
Thread  8: exec:        20069, sched:         0
Thread  9: exec:        19966, sched:         0
Thread 10: exec:        19988, sched:         0
Thread 11: exec:        19687, sched:         0
Thread 12: exec:        20766, sched:         0
Thread 13: exec:        19942, sched:         0
Thread 14: exec:        20164, sched:         0
Thread 15: exec:        19543, sched:         0
Thread 16: exec:        20408, sched:         0
Thread 17: exec:        20072, sched:         0
Thread 18: exec:        20106, sched:         0
Thread 19: exec:        19825, sched:         0
Thread 20: exec:        19598, sched:         0
Thread 21: exec:        20070, sched:         0
Thread 22: exec:        20100, sched:         0
Thread 23: exec:        20011, sched:         0
Thread 24: exec:        19836, sched:         0
Thread 25: exec:        19981, sched:         0
Thread 26: exec:        20300, sched:         0
Thread 27: exec:        19811, sched:         0
Thread 28: exec:        20056, sched:         0
Thread 29: exec:        20161, sched:         0
Thread 30: exec:        20220, sched:         0
Thread 31: exec:        20442, sched:         0
Thread 32: exec:        20020, sched:         0
Thread 33: exec:        19835, sched:         0
Thread 34: exec:        19775, sched:         0
Thread 35: exec:        19835, sched:         0
Thread 36: exec:        20024, sched:         0
Thread 37: exec:        19783, sched:         0
Thread 38: exec:        20082, sched:         0
Thread 39: exec:        19927, sched:         0
Thread 40: exec:        20245, sched:         0
Thread 41: exec:        20527, sched:         0
Thread 42: exec:        19806, sched:         0
Thread 43: exec:        19739, sched:         0
Thread 44: exec:        19913, sched:         0
Thread 45: exec:        19776, sched:         0
Thread 46: exec:        20336, sched:         0
Thread 47: exec:        20227, sched:         0
Thread 48: exec:        20107, sched:         0
Thread 49: exec:        20040, sched:         0

```
</details>

---
**Micro load, 50 threads, each task is executed 5 times**

```
Command line args: -load micro -threads 50 -exes 5
Run count: 5
Summary:
* 5 011 359 tasks per second;
* x10.874 of trivial scheduler;
```

<details><summary>Details</summary>

```
#### Canon task scheduler
== Aggregated report:
Exec per second min:       4 972 750
Exec per second median:    5 011 359
Exec per second max:       7 388 999

== Median report:
Microseconds per exec:          9.977333
Exec per second:                 5011359
Exec per second per thread:       100227
Mutex contention count:           257041
Thread  0: exec:       781829, sched:        17
Thread  1: exec:      1121575, sched:        21
Thread  2: exec:      1077850, sched:        20
Thread  3: exec:      1079662, sched:        19
Thread  4: exec:      1008583, sched:        16
Thread  5: exec:       883043, sched:        18
Thread  6: exec:      1432078, sched:        18
Thread  7: exec:       893556, sched:        20
Thread  8: exec:       903939, sched:        17
Thread  9: exec:      1175807, sched:        18
Thread 10: exec:       957466, sched:        18
Thread 11: exec:      1080494, sched:        17
Thread 12: exec:      1241511, sched:        18
Thread 13: exec:       892017, sched:        18
Thread 14: exec:      1015257, sched:        19
Thread 15: exec:       893239, sched:        16
Thread 16: exec:       757647, sched:        20
Thread 17: exec:      1181569, sched:        18
Thread 18: exec:       988410, sched:        18
Thread 19: exec:      1098678, sched:        18
Thread 20: exec:       863676, sched:        17
Thread 21: exec:      1118914, sched:        17
Thread 22: exec:       784755, sched:        18
Thread 23: exec:       933935, sched:        19
Thread 24: exec:      1276160, sched:        17
Thread 25: exec:      1110687, sched:        17
Thread 26: exec:       413280, sched:        20
Thread 27: exec:      1114442, sched:        18
Thread 28: exec:       926690, sched:        16
Thread 29: exec:      1088458, sched:        17
Thread 30: exec:      1103603, sched:        21
Thread 31: exec:      1200975, sched:        19
Thread 32: exec:      1108240, sched:        17
Thread 33: exec:       923233, sched:        19
Thread 34: exec:      1132828, sched:        16
Thread 35: exec:      1054299, sched:        17
Thread 36: exec:       939697, sched:        18
Thread 37: exec:       950159, sched:        17
Thread 38: exec:      1203626, sched:        19
Thread 39: exec:       827185, sched:        16
Thread 40: exec:       848276, sched:        17
Thread 41: exec:      1016079, sched:        17
Thread 42: exec:      1195653, sched:        18
Thread 43: exec:       895997, sched:        20
Thread 44: exec:      1088406, sched:        18
Thread 45: exec:      1068345, sched:        19
Thread 46: exec:       884298, sched:        17
Thread 47: exec:       733837, sched:        20
Thread 48: exec:      1023537, sched:        19
Thread 49: exec:       706520, sched:        20


#### Trivial task scheduler
== Aggregated report:
Exec per second min:         445 798
Exec per second median:      460 870
Exec per second max:         486 368

== Median report:
Microseconds per exec:        108.490294
Exec per second:                  460870
Exec per second per thread:         9217
Mutex contention count:          4617834
Thread  0: exec:       100244, sched:         0
Thread  1: exec:        98823, sched:         0
Thread  2: exec:        99033, sched:         0
Thread  3: exec:        99281, sched:         0
Thread  4: exec:       100244, sched:         0
Thread  5: exec:       100277, sched:         0
Thread  6: exec:        99751, sched:         0
Thread  7: exec:       100192, sched:         0
Thread  8: exec:        99446, sched:         0
Thread  9: exec:       100406, sched:         0
Thread 10: exec:       100514, sched:         0
Thread 11: exec:        99714, sched:         0
Thread 12: exec:        99622, sched:         0
Thread 13: exec:        99644, sched:         0
Thread 14: exec:        99865, sched:         0
Thread 15: exec:       100115, sched:         0
Thread 16: exec:       101323, sched:         0
Thread 17: exec:       100388, sched:         0
Thread 18: exec:        99990, sched:         0
Thread 19: exec:       100154, sched:         0
Thread 20: exec:        99219, sched:         0
Thread 21: exec:        99970, sched:         0
Thread 22: exec:        99144, sched:         0
Thread 23: exec:        99554, sched:         0
Thread 24: exec:       100443, sched:         0
Thread 25: exec:       100795, sched:         0
Thread 26: exec:        99488, sched:         0
Thread 27: exec:       100171, sched:         0
Thread 28: exec:       100017, sched:         0
Thread 29: exec:       100509, sched:         0
Thread 30: exec:        99916, sched:         0
Thread 31: exec:        99917, sched:         0
Thread 32: exec:        99977, sched:         0
Thread 33: exec:       100322, sched:         0
Thread 34: exec:       100157, sched:         0
Thread 35: exec:       100508, sched:         0
Thread 36: exec:        99605, sched:         0
Thread 37: exec:        99528, sched:         0
Thread 38: exec:       100341, sched:         0
Thread 39: exec:       100363, sched:         0
Thread 40: exec:       100047, sched:         0
Thread 41: exec:        99787, sched:         0
Thread 42: exec:        99535, sched:         0
Thread 43: exec:       100216, sched:         0
Thread 44: exec:       100199, sched:         0
Thread 45: exec:       100226, sched:         0
Thread 46: exec:       100012, sched:         0
Thread 47: exec:        99354, sched:         0
Thread 48: exec:       100460, sched:         0
Thread 49: exec:       101194, sched:         0

```
</details>
