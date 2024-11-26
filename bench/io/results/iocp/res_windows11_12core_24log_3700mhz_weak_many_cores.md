**OS**: Windows 11 Home 23H2 22631.3737<br>
**CPU**: AMD Ryzen 9 5900X 12-Core Processor 3.70 GHz, 12 Cores, 24 Logical Processors

---
**Basic**

```
Command line args: -thread_count 10 -connect_count_per_port 500 -message_payload_size 128 -message_int_count 5 -message_target_count 5000000
Run count: 5
Summary:
* 209 571 messages per second;
* x0.930 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         207 660
Message/sec(med) median:      209 571
Message/sec(med) max:         209 787

== Median report:

   Messages(total): 5018710
     Duration(sec): 26.250000
Message/sec(total): 191188
  Message/sec(med): 209571
  Message/sec(max): 210788
   Latency ms(med): 2.607049
   Latency ms(max): 2.653654

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         217 438
Message/sec(med) median:      225 316
Message/sec(med) max:         231 364

== Median report:

   Messages(total): 5020912
     Duration(sec): 23.828000
Message/sec(total): 210714
  Message/sec(med): 225316
  Message/sec(max): 240984
   Latency ms(med): 2.355898
   Latency ms(max): 2.493085
```
</details>

---
**Single thread**

```
Command line args: -thread_count 1 -connect_count_per_port 500 -message_payload_size 128 -message_int_count 5 -message_target_count 2000000
Run count: 3
Summary:
* 64 341 messages per second;
* x1.259 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:          62 153
Message/sec(med) median:       64 341
Message/sec(med) max:          64 644

== Median report:

   Messages(total): 2003890
     Duration(sec): 35.812000
Message/sec(total): 55955
  Message/sec(med): 64341
  Message/sec(max): 64810
   Latency ms(med): 8.499118
   Latency ms(max): 11.203871

#### boost::asio network
== Aggregated report:
Message/sec(med) min:          47 344
Message/sec(med) median:       51 121
Message/sec(med) max:          51 897

== Median report:

   Messages(total): 2006434
     Duration(sec): 43.297000
Message/sec(total): 46341
  Message/sec(med): 51121
  Message/sec(max): 62289
   Latency ms(med): 10.688741
   Latency ms(max): 13.429185
```
</details>

---
**10 threads 100kb messages**

```
Command line args: -thread_count 10 -connect_count_per_port 600 -message_payload_size 102400
Run count: 5
Summary:
* 26 289 messages per second;
* x1.525 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:          25 933
Message/sec(med) median:       26 289
Message/sec(med) max:          26 643

== Median report:

   Messages(total): 801691
     Duration(sec): 33.687000
Message/sec(total): 23798
  Message/sec(med): 26289
  Message/sec(max): 26459
   Latency ms(med): 24.843308
   Latency ms(max): 27.677855

#### boost::asio network
== Aggregated report:
Message/sec(med) min:          16 719
Message/sec(med) median:       17 235
Message/sec(med) max:          17 312

== Median report:

   Messages(total): 100563
     Duration(sec): 6.359000
Message/sec(total): 15814
  Message/sec(med): 17235
  Message/sec(max): 17620
   Latency ms(med): 37.777998
   Latency ms(max): 38.581990
```
</details>

---
**Parallel messages**

```
Command line args: -thread_count 10 -connect_count_per_port 500 -message_payload_size 128 -message_parallel_count 5 -message_target_count 20000000
Run count: 5
Summary:
* 997 494 messages per second;
* x0.854 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         987 949
Message/sec(med) median:      997 494
Message/sec(med) max:         999 332

== Median report:

   Messages(total): 20031700
     Duration(sec): 22.031000
Message/sec(total): 909250
  Message/sec(med): 997494
  Message/sec(max): 1011665
   Latency ms(med): 2.730070
   Latency ms(max): 2.819511

#### boost::asio network
== Aggregated report:
Message/sec(med) min:       1 157 859
Message/sec(med) median:    1 168 372
Message/sec(med) max:       1 170 332

== Median report:

   Messages(total): 20056850
     Duration(sec): 18.890000
Message/sec(total): 1061770
  Message/sec(med): 1168372
  Message/sec(max): 1171106
   Latency ms(med): 2.339200
   Latency ms(max): 2.455831
```
</details>

---
**Massively parallel**

```
Command line args: -thread_count 20 -connect_count_per_port 5000 -message_payload_size 128 -message_target_count 20000000
Run count: 5
Summary:
* 219 826 messages per second;
* x1.265 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         218 536
Message/sec(med) median:      219 826
Message/sec(med) max:         220 636

== Median report:

   Messages(total): 20017948
     Duration(sec): 109.297000
Message/sec(total): 183151
  Message/sec(med): 219826
  Message/sec(max): 279886
   Latency ms(med): 24.866879
   Latency ms(max): 2154.285714

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         173 593
Message/sec(med) median:      173 773
Message/sec(med) max:         175 029

== Median report:

   Messages(total): 20013042
     Duration(sec): 159.390000
Message/sec(total): 125560
  Message/sec(med): 173773
  Message/sec(max): 207545
   Latency ms(med): 30.908909
   Latency ms(max): 1032.921206
```
</details>
