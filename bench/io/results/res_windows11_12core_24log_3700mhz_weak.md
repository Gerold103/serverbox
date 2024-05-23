**OS**: Windows 11 Home 23H2 22631.3737<br>
**CPU**: AMD Ryzen 9 5900X 12-Core Processor 3.70 GHz, 12 Cores, 24 Logical Processors

---
**Basic**

```
Command line args: -thread_count 3 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 2500000
Run count: 3
Summary:
* 195 436 messages per second;
* x1.101 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         193 519
Message/sec(med) median:      195 436
Message/sec(med) max:         197 225

== Median report:

   Messages(total): 2501623
     Duration(sec): 14.016000
Message/sec(total): 178483
  Message/sec(med): 195436
  Message/sec(max): 196154
   Latency ms(med): 0.559572
   Latency ms(max): 0.563687

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         176 346
Message/sec(med) median:      177 584
Message/sec(med) max:         179 157

== Median report:

   Messages(total): 2515646
     Duration(sec): 15.531000
Message/sec(total): 161975
  Message/sec(med): 177584
  Message/sec(max): 181047
   Latency ms(med): 0.614552
   Latency ms(max): 0.644101
```
</details>

---
**Single thread**

```
Command line args: -thread_count 1 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 2000000
Run count: 3
Summary:
* 80 510 messages per second;
* x1.200 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:          80 330
Message/sec(med) median:       80 510
Message/sec(med) max:          80 977

== Median report:

   Messages(total): 2006655
     Duration(sec): 27.343000
Message/sec(total): 73388
  Message/sec(med): 80510
  Message/sec(max): 81671
   Latency ms(med): 1.357757
   Latency ms(max): 1.422896

#### boost::asio network
== Aggregated report:
Message/sec(med) min:          66 361
Message/sec(med) median:       67 091
Message/sec(med) max:          68 532

== Median report:

   Messages(total): 2004338
     Duration(sec): 32.266000
Message/sec(total): 62119
  Message/sec(med): 67091
  Message/sec(max): 71486
   Latency ms(med): 1.622270
   Latency ms(max): 1.657626
```
</details>

---
**3 threads 100kb messages**

```
Command line args: -thread_count 3 -connect_count_per_port 200 -message_payload_size 102400
Run count: 3
Summary:
* 28 368 messages per second;
* x2.112 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:          28 039
Message/sec(med) median:       28 368
Message/sec(med) max:          28 788

== Median report:

   Messages(total): 201354
     Duration(sec): 7.765000
Message/sec(total): 25930
  Message/sec(med): 28368
  Message/sec(max): 28599
   Latency ms(med): 7.691767
   Latency ms(max): 7.846596

#### boost::asio network
== Aggregated report:
Message/sec(med) min:          10 923
Message/sec(med) median:       13 431
Message/sec(med) max:          13 548

== Median report:

   Messages(total): 150910
     Duration(sec): 12.265000
Message/sec(total): 12304
  Message/sec(med): 13431
  Message/sec(max): 13657
   Latency ms(med): 16.256789
   Latency ms(max): 16.465809
```
</details>

---
**Parallel messages**

```
Command line args: -thread_count 3 -connect_count_per_port 100 -message_payload_size 128 -message_parallel_count 5 -message_target_count 20000000
Run count: 3
Summary:
* 730 270 messages per second;
* x1.066 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         686 030
Message/sec(med) median:      730 270
Message/sec(med) max:         789 652

== Median report:

   Messages(total): 20077660
     Duration(sec): 29.531000
Message/sec(total): 679884
  Message/sec(med): 730270
  Message/sec(max): 851047
   Latency ms(med): 0.747578
   Latency ms(max): 0.830214

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         634 260
Message/sec(med) median:      685 192
Message/sec(med) max:         685 524

== Median report:

   Messages(total): 20044200
     Duration(sec): 30.766000
Message/sec(total): 651504
  Message/sec(med): 685192
  Message/sec(max): 835312
   Latency ms(med): 0.763405
   Latency ms(max): 0.846458
```
</details>
