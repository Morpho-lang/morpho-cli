[comment]: # (String class help)
[version]: # (0.5)

# String
[tagstring]: # (String)

Strings represent textual information. They are written in Morpho like this:

    var a = "Hello world"

Strings may incorporate any valid UTF-8 sequence, and you can insert special characters using the "\" character including:

    "\b" // backspace 
    "\f" // form feed
    "\n" // newline
    "\r" // line feed
    "\t" // tab
    "\xhh" // specify an ASCII character with two hex digits
    "\uhhhh" // specify a unicode character with four hex digits
    "\Uhhhhhhhh" // specify a unicode character with eight hex digits

Unicode characters are converted to UTF8 by the morpho compiler.

You can also create strings using the constructor function `String`, which takes any number of parameters:

    var a = String("Hello", "World")

A very useful feature, called *string interpolation*, enables the results of any morpho expression can be interpolated into a string. Here, the values of `i` and `func(i)` will be inserted into the string as it is created:

    print "${i}: ${func(i)}"

To get an individual character, use index notatation

    print "morpho"[0]

You can loop over each character like this:

    for (c in "morpho") print c

Note that strings are immutable, and hence

    var a = "morpho"
    a[0] = 4

raises an error.

[showsubtopics]: #

## split
[tagsplit]: # (split)

The split method splits a String into a list of substrings. It takes one argument, which is a string of characters to use to split the string:

    print "1,2,3".split(",")

gives

    [ 1, 2, 3 ]
