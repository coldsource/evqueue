# evQueue

evQueue is an open source job scheduler and queueing engine. It features an event-driven C++ engine and a PHP / MySQL web control interface which provides tasks monitoring and creation. See the PHP frontend [here](https://github.com/coldsource/evqueue-frontend-php).

It provides both simple task execution and complex task chaining (workflow) using XML and XPath description. Workflow description includes output linking, conditions and loops. Queues management provides an easy way for task parallelization and resource control.

The network API provides an easy way, XML based, for synchronous or asynchronous workflow launching and control, allowing tasks to be launched from external applications or web pages.

evQueue is agentless (remote connection is made through SSH) and works on Linux environments. An agent is however provided to enable additional features when working over SSH.

We provide debian packages. For other distributions you can easily compile from source.

For documentation and binary download, [visit the official website!](http://www.evqueue.net/)

### Building

First, you need to install build dependencies :

```
sudo apt-get install git build-essential libmariadbclient-dev libxerces-c-dev libpcre++-dev libgit2-dev libwebsockets-dev libzip-dev
```

You can then build core using CMake :

``` 
mkdir build
cd build
cmake ..
make
```

Before launching evQueue, you need to set a configuration file and create a database *(see below)*.

### Configuring

We provide [sample configuration files](doc/config). For the impatients, start with **evqueue-mini.conf**, you just need to set your SQL configuration.

For a full description of the configuration file, please have a look at [configuration documentation](http://www.evqueue.net/doc/configuration-file).

### Creating a database

See [sql init](doc/sql).

### Running

```
./evqueue --config ../doc/config/evqueue-mini.conf
```

### Packages

If you are looking for pre-compiled packages, see [our debian repository](https://packagecloud.io/coldsource/evqueue).

If you want to build your own debian package from this source, use :

```
debuild -us -uc -b
```

### Docker

We also provide [docker images](https://hub.docker.com/u/coldsource).

Or you can [build your own images](https://github.com/coldsource/evqueue-docker).
