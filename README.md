deem
===
Key-Value ar Archive for Non-File Makefile Dependencies
---
The `deem` utility accepts an input stream of key-value pairs and stores them in an `ar` archive,
where the key is used as a filename and the value as file contents. In the subsequent invocations
`deem` will compare the new key-value streams against the values stored in the archive, and update
only those values which have changed. Because `make` depndencies can include files stored in `ar`
archives using the notation _archivename(filename)_, the `deem` utility actually enables such
dependencies to include arbitrary key-value pairs.

The original motivation behind `deem` was to automate static web-site builds based on content stored
in a database. It can be used, however, for other tasks as well, e.g., to make resource files, to
scan and digest external URLs, and any other dependency on non-file resources.

Keys and values must be a maximum of 255 bytes (note that utf-8 characters often consume more than 1 byte
per character) and cannot include whitespaces. In addition, keys must be valid unquoted and unescaped
filenames on the target operating system, and unique per stream. `deem` doesn't validate key nor
value conformance.

Usage
---
The values should be such that any change in the resource that requires a rebuild of targets depending
on it will be reflected in the value. The value can be for example:

- The content of the resource, if sufficiently short and has no whitespaces.

- A checksum or digest of the resource.

- The modification time stamp of the resouce if it is available. Such time stamp can be obtained,
for example, when the resource is web page, a file in a compressed archive, etc.

- A version number of the resource.

- Etc.

The key-value pairs are simply streamed as input to `deem`, whitespace seperated, and `deem` does
the rest.

Here's an example based on an sqlite3 database. We have a database called "catalog.db", and we
want to be able to create dependencies from each record in its "products" table. The example selects
all records and produces a report in a csv format. We use the value of the first field as the key
and the md5 digest of the entire record as the value. The key-value pairs are stored in an
`ar` archive called sql.a, and now we can reference in the Makefile each product by the
notation `sql.a(%)`:

    sqlite3 -csv catalog.db 'select * from products;'|
    xargs -I rec sh -c "echo 'rec'|openssl md5|sed -e 's/^.* /rec /' -e 's/,.* / /'"|
    deem sql.a

The next example is very similar except that now the records are stored in a MongoDB database,
enabling us to produce the key-value pairs using the database's built in Javascript engine:

    echo 'db.products.find().forEach(function(d){print(d.productname,hex_md5(tojson(d)))})'|
    mongo --quiet catalog|
    deem mongo.a

Command Line Options
---

`deem` accepts the following command line options:

- `-h` or `--help`: Prints a help message

- `-q` or `--quiet`: By default `deem` will print during its execution a stats summary that details how many
items are in the current `ar` archive, how many in the new stream, and how many items will be inserted/updated
or removed. The `--quiet` option will suppress this message. In addition `deem` prints a message when
it creates a new archive and when it is used in "test" mode (see next), `--quiet` will suppress these
messages as well.

- `-t` or `--test`: Running `deem` in test mode will cause `deem` to check for changes and print stats,
but the archive will not be touched, i.e. it will remain unchanged.

- `-i <filename>` or `--input <filename>`: Read the key-value pairs from &lt;filename&gt; rather than from stdin.

Note
---

The standard `make` utility targets can depend on files in `ar` archives, but not on archives in other formats.
However, since `ar` is only partially standardized and its archive format is highly implementation dependant,
the `deem` utility doesn't manipulate the archive directly, but instead has to call the `ar` utility to
perform all archive operations. This means that the `ar` utility seen by `deem` must be the same or compatible
with the `ar` utility that will be used by `make`.

Build
---

    git clone https://github.com/avnr/deem.git
    cd deem
    make

Tested on Linux (Debian), Windows + MinGW, Windows + Cygwin. Compilation fails on OpenBSD, you can make
it work by adding `-ftrampolines` to the compilation. If you've tested on additional platforms then please let
me know.

Test
---

A simple test covering all the update possibilities can be performed by:

    make test

To run it under MinGW, change in Makefile "./deem" to "deem".

License
---

MIT
