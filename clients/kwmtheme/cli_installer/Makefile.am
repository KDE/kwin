
# set the include path for X, qt and KDE
INCLUDES= $(all_includes)

####### This part is very kwmtheme specific
# you can add here more. This one gets installed 
bin_PROGRAMS = 	kwmtheme

# Which sources should be compiled for kwmtheme.
kwmtheme_SOURCES = main.cpp 

# the library search path. 
kwmtheme_LDFLAGS = $(all_libraries) $(KDE_RPATH)

# the libraries to link against. Be aware of the order. First the libraries,
# that depend on the following ones.
kwmtheme_LDADD   = $(LIB_KDECORE)

