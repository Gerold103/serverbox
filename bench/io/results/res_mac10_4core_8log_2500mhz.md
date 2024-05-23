**OS**: macOS Big Sur 11.7.10<br>
**CPU**: Quad-Core Intel Core i7 2.5 GHz, 4 cores, hyper-threading enabled

---
**Basic**

```
Command line args: -thread_count 3 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 2500000
Run count: 3
Summary:
* 146 932 messages per second;
* x1.501 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         142 939
Message/sec(med) median:      146 932
Message/sec(med) max:         166 093

== Median report:

   Messages(total): 2513321
     Duration(sec): 17.563000
Message/sec(total): 143103
  Message/sec(med): 146932
  Message/sec(max): 156787
   Latency ms(med): 0.694889
   Latency ms(max): 0.761669

#### boost::asio network
== Aggregated report:
Message/sec(med) min:          96 619
Message/sec(med) median:       97 883
Message/sec(med) max:          98 723

== Median report:

   Messages(total): 2509649
     Duration(sec): 26.832000
Message/sec(total): 93531
  Message/sec(med): 97883
  Message/sec(max): 102036
   Latency ms(med): 1.032069
   Latency ms(max): 1.450607
```
</details>

---
**Single thread**

```
Command line args: -thread_count 1 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 2000000
Run count: 3
Summary:
* 101 800 messages per second;
* x1.413 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:          97 492
Message/sec(med) median:      101 800
Message/sec(med) max:         103 656

== Median report:

   Messages(total): 2006404
     Duration(sec): 20.083000
Message/sec(total): 99905
  Message/sec(med): 101800
  Message/sec(max): 109913
   Latency ms(med): 0.999297
   Latency ms(max): 1.279684

#### boost::asio network
== Aggregated report:
Message/sec(med) min:          70 682
Message/sec(med) median:       72 026
Message/sec(med) max:          72 916

== Median report:

   Messages(total): 2001185
     Duration(sec): 28.252000
Message/sec(total): 70833
  Message/sec(med): 72026
  Message/sec(max): 75928
   Latency ms(med): 1.399461
   Latency ms(max): 1.595153
```
</details>

---
**3 threads 100kb messages**

```
Command line args: -thread_count 3 -connect_count_per_port 200 -message_payload_size 102400
Run count: 3
Summary:
* 12 752 messages per second;
* x1.452 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:          12 719
Message/sec(med) median:       12 752
Message/sec(med) max:          13 002

== Median report:

   Messages(total): 200146
     Duration(sec): 16.312000
Message/sec(total): 12269
  Message/sec(med): 12752
  Message/sec(max): 13347
   Latency ms(med): 15.962933
   Latency ms(max): 19.018281

#### boost::asio network
== Aggregated report:
Message/sec(med) min:           8 567
Message/sec(med) median:        8 780
Message/sec(med) max:           9 053

== Median report:

   Messages(total): 150925
     Duration(sec): 17.594000
Message/sec(total): 8578
  Message/sec(med): 8780
  Message/sec(max): 9337
   Latency ms(med): 22.705103
   Latency ms(max): 26.485947
```
</details>

---
**Parallel messages**

```
Command line args: -thread_count 3 -connect_count_per_port 100 -message_payload_size 128 -message_parallel_count 5 -message_target_count 20000000
Run count: 3
Summary:
* 712 243 messages per second;
* x1.536 of boost::asio;
```

<details><summary>Details</summary>

```
#### mg::aio network
== Aggregated report:
Message/sec(med) min:         688 768
Message/sec(med) median:      712 243
Message/sec(med) max:         713 392

== Median report:

   Messages(total): 20016525
     Duration(sec): 28.904000
Message/sec(total): 692517
  Message/sec(med): 712243
  Message/sec(max): 725235
   Latency ms(med): 0.718574
   Latency ms(max): 0.793306

#### boost::asio network
== Aggregated report:
Message/sec(med) min:         463 117
Message/sec(med) median:      463 717
Message/sec(med) max:         463 827

== Median report:

   Messages(total): 20045095
     Duration(sec): 43.940000
Message/sec(total): 456192
  Message/sec(med): 463717
  Message/sec(max): 478038
   Latency ms(med): 1.092702
   Latency ms(max): 1.174814
```
</details>
