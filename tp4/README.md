# TP4 - La pagination

Le but du TP est de bien comprendre la pagination.

Les fichiers [`cr.h`](../kernel/include/cr.h) et [`pagemem.h`](../kernel/include/pagemem.h) 
seront utiles (informations, de structures et de macros, etc.) pour la
résolution du TP.

## Prérequis théoriques

Concernant la pagination :

* Le bit 31 du registre CR0 permet d'activer ou désactiver la pagination (cf. macro `CR0_PG`).
* Le registre CR3 sert à stocker l'adresse du Page Directory (PGD).

## Notes sur les tableaux et pointeurs en C

```c
void fonction()
{
  int *tab_c = (int*)0x1234;
}
```
Le compilateur ne sait pas la taille de la zone mémoire adressée et
potentiellement vous pouvez accéder à toute la mémoire à partir de cette
adresse. Dans une application classique, déclarer un tel pointeur
provoquerait à coup sur un crash de l'application car l'adresse `0x1234`
n'est jamais disponible pour une application (ex. sous Linux). 

Dans notre noyau cela ne pause pas de problème, nous utilisons la
mémoire "physique" pour l'instant et nous n'avons pas de notion de tâche avec
des espaces d'adressage.

## Mise en place de schéma de pagination en identity mapping

**Q1\* : A l'aide de la fonction `get_cr3()`, afficher la valeur courante du
  registre CR3 dans `tp.c`.**

**Q2\* : Allouer un PGD de type `(pde32_t*)` à l'adresse physique `0x600000` et
  mettre à jour `CR3` avec cette adresse.**

**Q3\* : Modifier le registre CR0 de sorte à activer la pagination dans `tp.c`.
  Que se passe-t-il ? Pourquoi ?**

***reponse***
En mettant le bit PG de cr0 à 1, on active la pagination 
En executant qemu, on se retrouve avec une faute comme les entrees ne sont pas configurées : un mapping faux => page fault et au bout de 3 fautes, reset cest pour cela que l'emulateur restart <=> reset de la machine 

**Q4\* : Un certain nombre de choses restent à configurer avant l'activation de
  la pagination. Comme pour le PGD, allouer également une PTB de type `
  (pte32_t*)` à l'adresse `0x601000`.**

**Q5\* : Le but va être maintenant d'initialiser la mémoire virtuelle
  en "identity mapping" : les adresses virtuelles doivent être identiques aux
  adresses physiques. Pour cela :**

* **Bien étudier les plages d'adresses physiques occupées par le noyau
     (`readelf -e kernel.elf`, regarder les program headers).**
* **Préparer au moins une entrée dans le PGD pour la PTB.**
* **Préparer plusieurs entrées dans la PTB.**

***reponse***
Program Headers:
  Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
  LOAD           0x000094 0x00300000 0x00300000 0x0000c 0x0000c RWE 0x4
  LOAD           0x000000 0x00300010 0x00300010 0x00000 0x02000 RW  0x10
  LOAD           0x0000b0 0x00302010 0x00302010 0x02a00 0x03630 RWE 0x20

**Q6 : Une fois la pagination activée, essayer d'afficher le contenu d'une
  entrée de votre PTB. Que se passe-t-il ? Trouver la solution pour être
  capable de modifier les entrées de votre PTB une fois la pagination
  activée.**
***reponse***
page fault comme l'@ de ptb n'a pas de mapping => solution rajouter une page qui contient pdg et ptb

**Q7\* : Avant d'activer la pagination, on souhaiterait faire en sorte que
  l'adresse virtuelle `0xc0000000` permette de modifier votre PGD après
  activation de la pagination. Comment le réaliser ?**
***reponse***
On veut accéder au PGD après activation de la pagination via l’adresse virtuelle 0xc0000000
en mettant à l'indice 768 de pgd l'@ 0x00400000>>12 (la table qui contient le pgd) et puis de mettre dans l'entree 0 l'@ 0x600000>>12


## Quelques exercices supplémentaires de configuration spécifique

**Q8 : Faire en sorte que les adresses virtuelles `0x700000` et `0x7ff000`
  mappent l'adresse physique `0x2000`. Affichez la chaîne de caractères à ces
  adresses virtuelles.**
***reponse***
chaine a l'@ 0x700000 = /kernel.elf
chaine a l'@ 0x7ff000 = /kernel.elf

**Q9 : Effacer la première entrée du PGD. Que constatez-vous ? Expliquez
  pourquoi ?**
***reponse***
En remettant à 0 les champs de pgd[0] apres l'activation de la pagination, aucune caracteristique n'est visualisée. 
Si on ne l'active pas dès le debut => page fault 
//todo : voir ce qu'on cherche exactement en maz pgd[0]
