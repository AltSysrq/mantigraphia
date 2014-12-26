AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

noinst_DATA = ResourceFile.c

ResourceFile.c: resource-file.asn1
	$(ASN1C) -fcompound-names -fnative-types -gen-PER -R resource-file.asn1
	touch ResourceFile.c

clean: clean-am
	rm -f *.h *.c
