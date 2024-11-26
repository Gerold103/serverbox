**OS**: Ubuntu 24.04.1 LTS<br>
**CPU**: 12th Gen Intel(R) Core(TM) i7-12800HX 0.8-4.8GHz, 24 CPUs, 2 threads per core

---
**Basic**

```
Command line args: -thread_count 3 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 3500000
Run count: 3
Summary:
* 611 139 messages per second;
* x2.815 of io_uring;
```

<details><summary>Details</summary>

```
#### epoll scheduler
== Aggregated report:
Message/sec(med) min:         610 198
Message/sec(med) median:      611 139
Message/sec(med) max:         613 961

== Median report:

   Messages(total): 3549475
     Duration(sec): 5.808000
Message/sec(total): 611135
  Message/sec(med): 611139
  Message/sec(max): 630389
   Latency ms(med): 0.163588
   Latency ms(max): 0.175329

#### io_uring scheduler
== Aggregated report:
Message/sec(med) min:         217 035
Message/sec(med) median:      217 079
Message/sec(med) max:         221 791

== Median report:

   Messages(total): 3514503
     Duration(sec): 16.221000
Message/sec(total): 216663
  Message/sec(med): 217079
  Message/sec(max): 223472
   Latency ms(med): 0.461121
   Latency ms(max): 0.601515
```
</details>

---
**Single thread**

```
Command line args: -thread_count 1 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 3000000
Run count: 3
Summary:
* 253 887 messages per second;
* x1.041 of io_uring;
```

<details><summary>Details</summary>

```
#### epoll scheduler
== Aggregated report:
Message/sec(med) min:         252 522
Message/sec(med) median:      253 887
Message/sec(med) max:         257 373

== Median report:

   Messages(total): 3018797
     Duration(sec): 11.921000
Message/sec(total): 253233
  Message/sec(med): 253887
  Message/sec(max): 255099
   Latency ms(med): 0.394346
   Latency ms(max): 0.409378

#### io_uring scheduler
== Aggregated report:
Message/sec(med) min:         240 709
Message/sec(med) median:      243 945
Message/sec(med) max:         245 448

== Median report:

   Messages(total): 3023766
     Duration(sec): 12.418000
Message/sec(total): 243498
  Message/sec(med): 243945
  Message/sec(max): 248155
   Latency ms(med): 0.410375
   Latency ms(max): 0.454526
```
</details>

---
**3 threads 100kb messages**

```
Command line args: -thread_count 3 -connect_count_per_port 200 -message_payload_size 102400 -message_target_count 200000
Run count: 3
Summary:
* 35 565 messages per second;
* x2.716 of io_uring;
```

<details><summary>Details</summary>

```
#### epoll scheduler
== Aggregated report:
Message/sec(med) min:          35 023
Message/sec(med) median:       35 565
Message/sec(med) max:          37 689

== Median report:

   Messages(total): 201607
     Duration(sec): 5.608000
Message/sec(total): 35949
  Message/sec(med): 35565
  Message/sec(max): 37316
   Latency ms(med): 5.630207
   Latency ms(max): 6.115748

#### io_uring scheduler
== Aggregated report:
Message/sec(med) min:          12 771
Message/sec(med) median:       13 097
Message/sec(med) max:          13 282

== Median report:

   Messages(total): 200892
     Duration(sec): 15.529000
Message/sec(total): 12936
  Message/sec(med): 13097
  Message/sec(max): 13339
   Latency ms(med): 15.273018
   Latency ms(max): 17.761649
```
</details>

---
**Parallel messages**

```
Command line args: -thread_count 3 -connect_count_per_port 100 -message_payload_size 128 -message_parallel_count 5 -message_target_count 20000000
Run count: 3
Summary:
* 2 809 202 messages per second;
* x2.683 of io_uring;
```

<details><summary>Details</summary>

```
#### epoll scheduler
== Aggregated report:
Message/sec(med) min:       2 736 966
Message/sec(med) median:    2 809 202
Message/sec(med) max:       2 829 412

== Median report:

   Messages(total): 20113025
     Duration(sec): 7.111000
Message/sec(total): 2828438
  Message/sec(med): 2809202
  Message/sec(max): 2902660
   Latency ms(med): 0.177190
   Latency ms(max): 0.195870

#### io_uring scheduler
== Aggregated report:
Message/sec(med) min:       1 039 482
Message/sec(med) median:    1 046 845
Message/sec(med) max:       1 049 057

== Median report:

   Messages(total): 20044645
     Duration(sec): 19.127000
Message/sec(total): 1047976
  Message/sec(med): 1046845
  Message/sec(max): 1083113
   Latency ms(med): 0.477162
   Latency ms(max): 0.553485
```
</details>
