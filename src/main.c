#include <stdio.h>

#include "uthash.h"
struct my_struct *users = NULL;    /* important! initialize to NULL */
struct my_struct {
    int id;                    /* key */
    char name[10];
    UT_hash_handle hh;         /* makes this structure hashable */
};
/*声明哈希为NULL指针*/
void add_user(int user_id, char *name) {
    struct my_struct *s;
    /*重复性检查，当把两个相同key值的结构体添加到哈希表中时会报错*/
    HASH_FIND_INT(users, &user_id, s);  /* id already in the hash? */
    /*只有在哈希中不存在ID的情况下，我们才创建该项目并将其添加。否则，我们只修改已经存在的结构。*/
    if (s==NULL) {
      s = (struct my_struct *)malloc(sizeof *s);
      s->id = user_id;
      HASH_ADD_INT( users, id, s );  /* id: name of key field */
    }
    strcpy(s->name, name);
}
struct my_struct *find_user(int user_id) {
    struct my_struct *s;
    HASH_FIND_INT( users, &user_id, s );  /* s: output pointer */
    return s;
}
void delete_user(struct my_struct *user) {
    HASH_DEL(users, user);  /* user: pointer to deletee */
    free(user);             /* optional; it's up to you! */
}

/*
HASH_ITER(hh, users, current_user, tmp) {
    HASH_DEL(users,current_user);  
    free(current_user);          
  }

*/

int main()
{
	           
  
	return (0);
}