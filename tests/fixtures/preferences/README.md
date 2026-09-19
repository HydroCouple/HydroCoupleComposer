Settings files written by `test_preferences` and `test_preferencesdialog`.

Each test writes its own `.ini` here rather than to a temporary directory,
so what a test stored can be opened and read after the run. The files are
recreated on every run and are safe to delete.
