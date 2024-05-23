**OS**: Ubuntu 22.04.4 LTS<br>
**CPU**: 12th Gen Intel(R) Core(TM) i7-12800HX 0.8-4.8GHz, 24 CPUs, 2 threads per core

---
**Basic**

```
Command line args: -thread_count 5 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 5000000
Run count: 5
Summary:
* 962 631 messages per second;
* x1.549 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         936 950
Message/sec(med) median:      962 631
Message/sec(med) max:         966 197

== Median report:

   Messages(total): 5046319
     Duration(sec): 5.304000
Message/sec(total): 951417
  Message/sec(med): 962631
  Message/sec(max): 979690
   Latency ms(med): 0.103917
   Latency ms(max): 0.326609

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         607 149
Message/sec(med) median:      621 455
Message/sec(med) max:         634 508

== Median report:

   Messages(total): 5026974
     Duration(sec): 8.107000
Message/sec(total): 620078
  Message/sec(med): 621455
  Message/sec(max): 633177
   Latency ms(med): 0.160041
   Latency ms(max): 0.164533
```
</details>

---
**Single thread**

```
Command line args: -thread_count 1 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 2000000
Run count: 5
Summary:
* 330 202 messages per second;
* x1.459 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         328 713
Message/sec(med) median:      330 202
Message/sec(med) max:         331 758

== Median report:

   Messages(total): 2027516
     Duration(sec): 6.204000
Message/sec(total): 326807
  Message/sec(med): 330202
  Message/sec(max): 332413
   Latency ms(med): 0.302987
   Latency ms(max): 0.444898

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         223 622
Message/sec(med) median:      226 385
Message/sec(med) max:         226 921

== Median report:

   Messages(total): 2007201
     Duration(sec): 8.907000
Message/sec(total): 225350
  Message/sec(med): 226385
  Message/sec(max): 227019
   Latency ms(med): 0.441120
   Latency ms(max): 0.451560
```
</details>

---
**5 threads 100kb messages**

```
Command line args: -thread_count 5 -connect_count_per_port 200 -message_payload_size 102400
Run count: 5
Summary:
* 51 105 messages per second;
* x4.444 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:          50 036
Message/sec(med) median:       51 105
Message/sec(med) max:          51 451

== Median report:

   Messages(total): 800427
     Duration(sec): 15.812000
Message/sec(total): 50621
  Message/sec(med): 51105
  Message/sec(max): 52767
   Latency ms(med): 3.903223
   Latency ms(max): 13.473545

#### boost::asio network
== Aggregated report:
Message/sec(med) min:          10 743
Message/sec(med) median:       11 501
Message/sec(med) max:          11 815

== Median report:

   Messages(total): 100672
     Duration(sec): 9.008000
Message/sec(total): 11175
  Message/sec(med): 11501
  Message/sec(max): 12452
   Latency ms(med): 17.407676
   Latency ms(max): 27.633073
```
</details>

---
**Parallel messages**

```
Command line args: -thread_count 5 -connect_count_per_port 100 -message_payload_size 128 -message_parallel_count 5 -message_target_count 20000000
Run count: 5
Summary:
* 4 001 805 messages per second;
* x1.917 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:       3 899 441
Message/sec(med) median:    4 001 805
Message/sec(med) max:       4 122 601

== Median report:

   Messages(total): 20340485
     Duration(sec): 5.205000
Message/sec(total): 3907874
  Message/sec(med): 4001805
  Message/sec(max): 4058287
   Latency ms(med): 0.125062
   Latency ms(max): 0.515268

#### boost::asio network
== Aggregated report:
Message/sec(med) min:       1 665 533
Message/sec(med) median:    2 087 534
Message/sec(med) max:       2 921 384

== Median report:

   Messages(total): 20032435
     Duration(sec): 9.006000
Message/sec(total): 2224343
  Message/sec(med): 2087534
  Message/sec(max): 2895183
   Latency ms(med): 0.178412
   Latency ms(max): 0.337385
```
</details>
