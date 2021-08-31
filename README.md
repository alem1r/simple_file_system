# simple_file_system

Instructions to run:

```
    make                
    cd shell       
    ./shell
```

Il progetto consinste in un'implementazione di un file-system in user space. Le componenti principali sono:

1) bitmap: gestisce i blocchi su disco

2) disk_driver: implementazione di un disco gestito a blocchi utilizzando un file

3) simplefs: implementazione del file system, ogni file o directory è gestita a blocchi. Inizialmente viene creato un blocco e se esso non è sufficiente a mantenere i dati scritti in futuro, altri blocchi saranno automaticamente allocati.