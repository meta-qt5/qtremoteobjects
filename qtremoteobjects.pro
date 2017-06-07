CONFIG += examples_need_tools tests_need_tools
load(qt_parts)

SUBDIRS += mkspecs

tools-only {
    sub_tools.depends -= sub_src
    SUBDIRS = sub_tools
}
