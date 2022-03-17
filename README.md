# ifshow & ifnetshow

Simple tools to display systems network interfaces and their IPv4 and IPv6.

**ifshow** is for local systems, you can show every interfaces and apply filters on their name.

With **ifnetshow** you can run a server on a system and remotely requests it's interfaces list.

## Compile

Debian requirements : `sudo apt install gcc`

```
gcc ifshow.c -o ifshow
gcc ifnetshow.c -o ifnetshow
```

## Usage

### ifshow

```
$ ifshow -h
Usage: ifshow [ OPTION ]
Where OPTION := { -a[ll] | -i[nterface] ifname }
```

### ifnetshow

```
$ ifnetshow -h
Usage: ifnetshow [ COMMAND ] [ OPTION ]
Where COMMAND := { -s[tart] | -n[etwork] address }
      OPTION  := { -a[ll]   | -i[nterface] ifname }
```
