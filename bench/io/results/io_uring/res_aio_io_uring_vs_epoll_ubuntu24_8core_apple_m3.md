**OS**: Ubuntu 24.04.1 LTS<br>
**CPU**: Apple M3 8 cores

---
**Basic**

```
Command line args: -thread_count 3 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 3500000
Run count: 3
Summary:
* 622 811 messages per second;
* x1.830 of io_uring;
```

<details><summary>Details</summary>

```
#### epoll scheduler
== Aggregated report:
Message/sec(med) min:         563 440
Message/sec(med) median:      622 811
Message/sec(med) max:         643 547

== Median report:

   Messages(total): 3510364
     Duration(sec): 5.592000
Message/sec(total): 627747
  Message/sec(med): 622811
  Message/sec(max): 648905
   Latency ms(med): 0.158673
   Latency ms(max): 0.166710

#### io_uring scheduler
== Aggregated report:
Message/sec(med) min:         334 604
Message/sec(med) median:      340 348
Message/sec(med) max:         342 363

== Median report:

   Messages(total): 3529004
     Duration(sec): 10.512000
Message/sec(total): 335711
  Message/sec(med): 340348
  Message/sec(max): 342119
   Latency ms(med): 0.296803
   Latency ms(max): 0.303746
```
</details>

---
**Single thread**

```
Command line args: -thread_count 1 -connect_count_per_port 100 -message_payload_size 128 -message_int_count 5 -message_target_count 3000000
Run count: 3
Summary:
* 494 013 messages per second;
* x1.398 of io_uring;
```

<details><summary>Details</summary>

```
#### epoll scheduler
== Aggregated report:
Message/sec(med) min:         492 080
Message/sec(med) median:      494 013
Message/sec(med) max:         494 404

== Median report:

   Messages(total): 3000812
     Duration(sec): 6.209000
Message/sec(total): 483300
  Message/sec(med): 494013
  Message/sec(max): 496573
   Latency ms(med): 0.206746
   Latency ms(max): 0.212882

#### io_uring scheduler
== Aggregated report:
Message/sec(med) min:         348 945
Message/sec(med) median:      353 400
Message/sec(med) max:         355 983

== Median report:

   Messages(total): 3031085
     Duration(sec): 8.769000
Message/sec(total): 345659
  Message/sec(med): 353400
  Message/sec(max): 359627
   Latency ms(med): 0.289152
   Latency ms(max): 0.295871
```
</details>

---
**3 threads 100kb messages**

```
Command line args: -thread_count 3 -connect_count_per_port 200 -message_payload_size 102400 -message_target_count 200000
Run count: 3
Summary:
* 29 924 messages per second;
* x1.811 of io_uring;
```

<details><summary>Details</summary>

```
#### epoll scheduler
== Aggregated report:
Message/sec(med) min:          27 764
Message/sec(med) median:       29 924
Message/sec(med) max:          30 122

== Median report:

   Messages(total): 200845
     Duration(sec): 6.806000
Message/sec(total): 29509
  Message/sec(med): 29924
  Message/sec(max): 31035
   Latency ms(med): 6.756463
   Latency ms(max): 7.093313

#### io_uring scheduler
== Aggregated report:
Message/sec(med) min:          16 393
Message/sec(med) median:       16 525
Message/sec(med) max:          17 532

== Median report:

   Messages(total): 200967
     Duration(sec): 12.322000
Message/sec(total): 16309
  Message/sec(med): 16525
  Message/sec(max): 17284
   Latency ms(med): 12.241331
   Latency ms(max): 14.351094
```
</details>

---
**Parallel messages**

```
Command line args: -thread_count 3 -connect_count_per_port 100 -message_payload_size 128 -message_parallel_count 5 -message_target_count 20000000
Run count: 3
Summary:
* 2 827 085 messages per second;
* x1.755 of io_uring;
```

<details><summary>Details</summary>

```
#### epoll scheduler
== Aggregated report:
Message/sec(med) min:       2 719 850
Message/sec(med) median:    2 827 085
Message/sec(med) max:       2 848 640

== Median report:

   Messages(total): 20270890
     Duration(sec): 7.285000
Message/sec(total): 2782551
  Message/sec(med): 2827085
  Message/sec(max): 2850727
   Latency ms(med): 0.178999
   Latency ms(max): 0.182353

#### io_uring scheduler
== Aggregated report:
Message/sec(med) min:       1 597 967
Message/sec(med) median:    1 611 145
Message/sec(med) max:       1 614 702

== Median report:

   Messages(total): 20097575
     Duration(sec): 12.879000
Message/sec(total): 1560491
  Message/sec(med): 1611145
  Message/sec(max): 1637125
   Latency ms(med): 0.313043
   Latency ms(max): 0.344839
```
</details>
