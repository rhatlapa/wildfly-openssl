.SUFFIXES: .dll

all: target\classes\wfssl.dll
OBJECTS = target\alpn.obj target\clientcert.obj target\options.obj target\session.obj target\ssl.obj target\threads.obj target\util.obj

{../libwfssl/src}.c{target}.obj:
    cl $<  /Fo$@ -I..\libwfssl\include -I"$(JAVA_HOME)\include" -I"$(JAVA_HOME)\include\win32" -IC:\OpenSSL-Win64\include /LD /c

target:
  mkdir target


target\classes: target
  mkdir target\classes

target\classes\wfssl.dll: target\classes $(OBJECTS)
   link $(OBJECTS)  /machine:AMD64 /DLL -out:target\classes\wfssl.dll