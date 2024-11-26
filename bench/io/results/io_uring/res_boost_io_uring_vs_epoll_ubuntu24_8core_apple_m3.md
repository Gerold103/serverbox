**OS**: Ubuntu 24.04.1 LTS<br>
**CPU**: Apple M3 8 cores

---
**Basic**

```
Command line args: -thread_count 3 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 3500000
Run count: 3
Summary:
* 534 676 messages per second;
* x1.870 of io_uring;
```

<details><summary>Details</summary>

```
#### epoll scheduler
== Aggregated report:
Message/sec(med) min:         528 981
Message/sec(med) median:      534 676
Message/sec(med) max:         545 657

== Median report:

   Messages(total): 3555647
     Duration(sec): 6.665000
Message/sec(total): 533480
  Message/sec(med): 534676
  Message/sec(max): 541789
   Latency ms(med): 0.187957
   Latency ms(max): 0.189362

#### io_uring scheduler
== Aggregated report:
Message/sec(med) min:         285 629
Message/sec(med) median:      285 963
Message/sec(med) max:         286 439

== Median report:

   Messages(total): 3519226
     Duration(sec): 12.427000
Message/sec(total): 283191
  Message/sec(med): 285963
  Message/sec(max): 288531
   Latency ms(med): 0.352745
   Latency ms(max): 0.360821
```
</details>

---
**Single thread**

```
Command line args: -thread_count 1 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 3000000
Run count: 3
Summary:
* 292 813 messages per second;
* x0.935 of io_uring;
```

<details><summary>Details</summary>

```
#### epoll scheduler
== Aggregated report:
Message/sec(med) min:         287 110
Message/sec(med) median:      292 813
Message/sec(med) max:         296 198

== Median report:

   Messages(total): 3010991
     Duration(sec): 10.427000
Message/sec(total): 288768
  Message/sec(med): 292813
  Message/sec(max): 297086
   Latency ms(med): 0.343970
   Latency ms(max): 0.348846

#### io_uring scheduler
== Aggregated report:
Message/sec(med) min:         312 820
Message/sec(med) median:      313 153
Message/sec(med) max:         314 380

== Median report:

   Messages(total): 3028741
     Duration(sec): 9.716000
Message/sec(total): 311727
  Message/sec(med): 313153
  Message/sec(max): 321048
   Latency ms(med): 0.320853
   Latency ms(max): 0.327553
```
</details>

---
**3 threads 100kb messages**

```
Command line args: -thread_count 3 -connect_count_per_port 200 -message_payload_size 102400 -message_target_count 200000
Run count: 3
Summary:
* 22 446 messages per second;
* x1.994 of io_uring;
```

<details><summary>Details</summary>

```
#### epoll scheduler
== Aggregated report:
Message/sec(med) min:          22 421
Message/sec(med) median:       22 446
Message/sec(med) max:          22 454

== Median report:

   Messages(total): 201034
     Duration(sec): 9.181000
Message/sec(total): 21896
  Message/sec(med): 22446
  Message/sec(max): 22668
   Latency ms(med): 8.992140
   Latency ms(max): 11.766725

#### io_uring scheduler
== Aggregated report:
Message/sec(med) min:          11 041
Message/sec(med) median:       11 258
Message/sec(med) max:          11 522

== Median report:

   Messages(total): 200283
     Duration(sec): 18.080000
Message/sec(total): 11077
  Message/sec(med): 11258
  Message/sec(max): 11471
   Latency ms(med): 17.933863
   Latency ms(max): 18.935533
```
</details>

---
**Parallel messages**

```
Command line args: -thread_count 3 -connect_count_per_port 100 -message_payload_size 128 -message_parallel_count 5 -message_target_count 20000000
Run count: 3
Summary:
* 2 339 822 messages per second;
* x1.744 of io_uring;
```

<details><summary>Details</summary>

```
#### epoll scheduler
== Aggregated report:
Message/sec(med) min:       2 322 146
Message/sec(med) median:    2 339 822
Message/sec(med) max:       2 388 937

== Median report:

   Messages(total): 20079730
     Duration(sec): 8.693000
Message/sec(total): 2309873
  Message/sec(med): 2339822
  Message/sec(max): 2374222
   Latency ms(med): 0.215233
   Latency ms(max): 0.220279

#### io_uring scheduler
== Aggregated report:
Message/sec(med) min:       1 340 102
Message/sec(med) median:    1 341 585
Message/sec(med) max:       1 350 452

== Median report:

   Messages(total): 20055970
     Duration(sec): 15.477000
Message/sec(total): 1295856
  Message/sec(med): 1341585
  Message/sec(max): 1362690
   Latency ms(med): 0.376221
   Latency ms(max): 0.476162
```
</details>
