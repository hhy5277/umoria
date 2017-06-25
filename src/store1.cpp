// Copyright (c) 1989-2008 James E. Wilson, Robert A. Koeneke, David J. Grabiner
//
// Umoria is free software released under a GPL v2 license and comes with
// ABSOLUTELY NO WARRANTY. See https://www.gnu.org/licenses/gpl-2.0.html
// for further details.

// Store code, updating store inventory, pricing objects

#include "headers.h"
#include "externs.h"

static void insert_store(int, int, int32_t, Inventory_t *);

static void store_create(int store_num, int16_t max_cost);

static int32_t getWeaponArmorBuyPrice(Inventory_t *i_ptr);
static int32_t getAmmoBuyPrice(Inventory_t *i_ptr);
static int32_t getPotionScrollBuyPrice(Inventory_t *i_ptr);
static int32_t getFoodBuyPrice(Inventory_t *i_ptr);
static int32_t getRingAmuletBuyPrice(Inventory_t *i_ptr);
static int32_t getWandStaffBuyPrice(Inventory_t *i_ptr);
static int32_t getPickShovelBuyPrice(Inventory_t *i_ptr);

// Returns the value for any given object -RAK-
int32_t item_value(Inventory_t *i_ptr) {
    int32_t value;

    if (i_ptr->ident & ID_DAMD) {
        // don't purchase known cursed items
        value = 0;
    } else if ((i_ptr->tval >= TV_BOW && i_ptr->tval <= TV_SWORD) || (i_ptr->tval >= TV_BOOTS && i_ptr->tval <= TV_SOFT_ARMOR)) {
        value = getWeaponArmorBuyPrice(i_ptr);
    } else if (i_ptr->tval >= TV_SLING_AMMO && i_ptr->tval <= TV_SPIKE) {
        value = getAmmoBuyPrice(i_ptr);
    } else if (i_ptr->tval == TV_SCROLL1 || i_ptr->tval == TV_SCROLL2 || i_ptr->tval == TV_POTION1 || i_ptr->tval == TV_POTION2) {
        value = getPotionScrollBuyPrice(i_ptr);
    } else if (i_ptr->tval == TV_FOOD) {
        value = getFoodBuyPrice(i_ptr);
    } else if (i_ptr->tval == TV_AMULET || i_ptr->tval == TV_RING) {
        value = getRingAmuletBuyPrice(i_ptr);
    } else if (i_ptr->tval == TV_STAFF || i_ptr->tval == TV_WAND) {
        value = getWandStaffBuyPrice(i_ptr);
    } else if (i_ptr->tval == TV_DIGGING) {
        value = getPickShovelBuyPrice(i_ptr);
    } else {
        value = i_ptr->cost;
    }

    // Multiply value by number of items if it is a group stack item.
    // Do not include torches here.
    if (i_ptr->subval > ITEM_GROUP_MIN) {
        value = value * i_ptr->number;
    }

    return value;
}

static int32_t getWeaponArmorBuyPrice(Inventory_t *i_ptr) {
    if (!known2_p(i_ptr)) {
        return object_list[i_ptr->index].cost;
    }

    if (i_ptr->tval >= TV_BOW && i_ptr->tval <= TV_SWORD) {
        if (i_ptr->tohit < 0 || i_ptr->todam < 0 || i_ptr->toac < 0) {
            return 0;
        }

        return i_ptr->cost + (i_ptr->tohit + i_ptr->todam + i_ptr->toac) * 100;
    }

    if (i_ptr->toac < 0) {
        return 0;
    }

    return i_ptr->cost + i_ptr->toac * 100;
}

static int32_t getAmmoBuyPrice(Inventory_t *i_ptr) {
    if (!known2_p(i_ptr)) {
        return object_list[i_ptr->index].cost;
    }

    if (i_ptr->tohit < 0 || i_ptr->todam < 0 || i_ptr->toac < 0) {
        return 0;
    }

    // use 5, because missiles generally appear in groups of 20,
    // so 20 * 5 == 100, which is comparable to weapon bonus above
    return i_ptr->cost + (i_ptr->tohit + i_ptr->todam + i_ptr->toac) * 5;
}

static int32_t getPotionScrollBuyPrice(Inventory_t *i_ptr) {
    if (!known1_p(i_ptr)) {
        return 20;
    }

    return i_ptr->cost;
}

static int32_t getFoodBuyPrice(Inventory_t *i_ptr) {
    if (i_ptr->subval < ITEM_SINGLE_STACK_MIN + MAX_MUSH && !known1_p(i_ptr)) {
        return 1;
    }

    return i_ptr->cost;
}

static int32_t getRingAmuletBuyPrice(Inventory_t *i_ptr) {
    // player does not know what type of ring/amulet this is
    if (!known1_p(i_ptr)) {
        return 45;
    }

    // player knows what type of ring, but does not know whether it
    // is cursed or not, if refuse to buy cursed objects here, then
    // player can use this to 'identify' cursed objects
    if (!known2_p(i_ptr)) {
        return object_list[i_ptr->index].cost;
    }

    return i_ptr->cost;
}

static int32_t getWandStaffBuyPrice(Inventory_t *i_ptr) {
    if (!known1_p(i_ptr)) {
        if (i_ptr->tval == TV_WAND) {
            return 50;
        }

        return 70;
    }

    if (known2_p(i_ptr)) {
        return i_ptr->cost + (i_ptr->cost / 20) * i_ptr->p1;
    }

    return i_ptr->cost;
}

static int32_t getPickShovelBuyPrice(Inventory_t *i_ptr) {
    if (!known2_p(i_ptr)) {
        return object_list[i_ptr->index].cost;
    }

    if (i_ptr->p1 < 0) {
        return 0;
    }

    // some digging tools start with non-zero p1 values, so only
    // multiply the plusses by 100, make sure result is positive
    int32_t value = i_ptr->cost + (i_ptr->p1 - object_list[i_ptr->index].p1) * 100;

    if (value < 0) {
        value = 0;
    }

    return value;
}

// Asking price for an item -RAK-
int32_t sell_price(int snum, int32_t *max_sell, int32_t *min_sell, Inventory_t *item) {
    int32_t price = item_value(item);

    // check `item->cost` in case it is cursed, check `price` in case it is damaged
    // don't let the item get into the store inventory
    if (item->cost < 1 || price < 1) {
        return 0;
    }

    Owner_t *owner = &owners[stores[snum].owner];

    price = price * rgold_adj[owner->owner_race][py.misc.prace] / 100;
    if (price < 1) {
        price = 1;
    }

    *max_sell = price * owner->max_inflate / 100;
    *min_sell = price * owner->min_inflate / 100;

    if (*min_sell > *max_sell) {
        *min_sell = *max_sell;
    }

    return price;
}

// Check to see if he will be carrying too many objects -RAK-
bool store_check_num(Inventory_t *t_ptr, int store_num) {
    Store_t *s_ptr = &stores[store_num];

    if (s_ptr->store_ctr < STORE_INVEN_MAX) {
        return true;
    }

    if (t_ptr->subval < ITEM_SINGLE_STACK_MIN) {
        return false;
    }

    bool store_check = false;

    for (int i = 0; i < s_ptr->store_ctr; i++) {
        Inventory_t *i_ptr = &s_ptr->store_inven[i].sitem;

        // note: items with subval of gte ITEM_SINGLE_STACK_MAX only stack
        // if their subvals match
        if (i_ptr->tval == t_ptr->tval && i_ptr->subval == t_ptr->subval && (int) (i_ptr->number + t_ptr->number) < 256 &&
            (t_ptr->subval < ITEM_GROUP_MIN || i_ptr->p1 == t_ptr->p1)) {
            store_check = true;
        }
    }

    return store_check;
}

// Insert INVEN_MAX at given location
static void insert_store(int store_num, int pos, int32_t icost, Inventory_t *i_ptr) {
    Store_t *s_ptr = &stores[store_num];

    for (int i = s_ptr->store_ctr - 1; i >= pos; i--) {
        s_ptr->store_inven[i + 1] = s_ptr->store_inven[i];
    }

    s_ptr->store_inven[pos].sitem = *i_ptr;
    s_ptr->store_inven[pos].scost = -icost;
    s_ptr->store_ctr++;
}

// Add the item in INVEN_MAX to stores inventory. -RAK-
void store_carry(int store_num, int *ipos, Inventory_t *t_ptr) {
    *ipos = -1;

    int32_t icost, dummy;
    if (sell_price(store_num, &icost, &dummy, t_ptr) < 1) {
        return;
    }

    Store_t *s_ptr = &stores[store_num];

    int item_val = 0;
    int item_num = t_ptr->number;
    int typ = t_ptr->tval;
    int subt = t_ptr->subval;

    bool flag = false;
    do {
        Inventory_t *i_ptr = &s_ptr->store_inven[item_val].sitem;

        if (typ == i_ptr->tval) {
            if (subt == i_ptr->subval && // Adds to other item
                subt >= ITEM_SINGLE_STACK_MIN && (subt < ITEM_GROUP_MIN || i_ptr->p1 == t_ptr->p1)) {
                *ipos = item_val;
                i_ptr->number += item_num;

                // must set new scost for group items, do this only for items
                // strictly greater than group_min, not for torches, this
                // must be recalculated for entire group
                if (subt > ITEM_GROUP_MIN) {
                    (void) sell_price(store_num, &icost, &dummy, i_ptr);
                    s_ptr->store_inven[item_val].scost = -icost;
                } else if (i_ptr->number > 24) {
                    // must let group objects (except torches) stack over 24
                    // since there may be more than 24 in the group
                    i_ptr->number = 24;
                }
                flag = true;
            }
        } else if (typ > i_ptr->tval) { // Insert into list
            insert_store(store_num, item_val, icost, t_ptr);
            flag = true;
            *ipos = item_val;
        }
        item_val++;
    } while (item_val < s_ptr->store_ctr && !flag);

    // Becomes last item in list
    if (!flag) {
        insert_store(store_num, (int) s_ptr->store_ctr, icost, t_ptr);
        *ipos = s_ptr->store_ctr - 1;
    }
}

// Destroy an item in the stores inventory.  Note that if
// "one_of" is false, an entire slot is destroyed -RAK-
void store_destroy(int store_num, int item_val, bool one_of) {
    Store_t *s_ptr = &stores[store_num];
    Inventory_t *i_ptr = &s_ptr->store_inven[item_val].sitem;

    int number;

    // for single stackable objects, only destroy one half on average,
    // this will help ensure that general store and alchemist have
    // reasonable selection of objects
    if (i_ptr->subval >= ITEM_SINGLE_STACK_MIN && i_ptr->subval <= ITEM_SINGLE_STACK_MAX) {
        if (one_of) {
            number = 1;
        } else {
            number = randint((int) i_ptr->number);
        }
    } else {
        number = i_ptr->number;
    }

    if (number != i_ptr->number) {
        i_ptr->number -= number;
    } else {
        for (int j = item_val; j < s_ptr->store_ctr - 1; j++) {
            s_ptr->store_inven[j] = s_ptr->store_inven[j + 1];
        }
        invcopy(&s_ptr->store_inven[s_ptr->store_ctr - 1].sitem, OBJ_NOTHING);
        s_ptr->store_inven[s_ptr->store_ctr - 1].scost = 0;
        s_ptr->store_ctr--;
    }
}

// Initializes the stores with owners -RAK-
void store_init() {
    int i = MAX_OWNERS / MAX_STORES;

    for (int j = 0; j < MAX_STORES; j++) {
        Store_t *s_ptr = &stores[j];

        s_ptr->owner = (uint8_t) (MAX_STORES * (randint(i) - 1) + j);
        s_ptr->insult_cur = 0;
        s_ptr->store_open = 0;
        s_ptr->store_ctr = 0;
        s_ptr->good_buy = 0;
        s_ptr->bad_buy = 0;

        for (int k = 0; k < STORE_INVEN_MAX; k++) {
            invcopy(&s_ptr->store_inven[k].sitem, OBJ_NOTHING);
            s_ptr->store_inven[k].scost = 0;
        }
    }
}

// Creates an item and inserts it into store's inven -RAK-
static void store_create(int store_num, int16_t max_cost) {
    int cur_pos = popt();

    for (int tries = 0; tries <= 3; tries++) {
        int i = store_choices[store_num][randint(STORE_CHOICES) - 1];
        invcopy(&t_list[cur_pos], i);
        magic_treasure(cur_pos, OBJ_TOWN_LEVEL);

        Inventory_t *t_ptr = &t_list[cur_pos];

        if (store_check_num(t_ptr, store_num)) {
            // Item must be good: cost > 0.
            if (t_ptr->cost > 0 && t_ptr->cost < max_cost) {
                // equivalent to calling ident_spell(),
                // except will not change the objects_identified array.
                store_bought(t_ptr);

                int dummy;
                store_carry(store_num, &dummy, t_ptr);

                tries = 10;
            }
        }
    }

    pusht((uint8_t) cur_pos);
}

// Initialize and up-keep the store's inventory. -RAK-
void store_maint() {
    for (int store_id = 0; store_id < MAX_STORES; store_id++) {
        Store_t *s_ptr = &stores[store_id];

        s_ptr->insult_cur = 0;
        if (s_ptr->store_ctr >= STORE_MIN_INVEN) {
            int j = randint(STORE_TURN_AROUND);
            if (s_ptr->store_ctr >= STORE_MAX_INVEN) {
                j += 1 + s_ptr->store_ctr - STORE_MAX_INVEN;
            }
            while (--j >= 0) {
                store_destroy(store_id, randint((int) s_ptr->store_ctr) - 1, false);
            }
        }

        if (s_ptr->store_ctr <= STORE_MAX_INVEN) {
            int j = randint(STORE_TURN_AROUND);
            if (s_ptr->store_ctr < STORE_MIN_INVEN) {
                j += STORE_MIN_INVEN - s_ptr->store_ctr;
            }

            int16_t max_cost = owners[s_ptr->owner].max_cost;

            while (--j >= 0) {
                store_create(store_id, max_cost);
            }
        }
    }
}

// eliminate need to bargain if player has haggled well in the past -DJB-
bool noneedtobargain(int store_num, int32_t minprice) {
    Store_t *s_ptr = &stores[store_num];

    if (s_ptr->good_buy == MAX_SHORT) {
        return true;
    }

    int bargain_record = (s_ptr->good_buy - 3 * s_ptr->bad_buy - 5);

    return ((bargain_record > 0) && ((int32_t) bargain_record * (int32_t) bargain_record > minprice / 50));
}

// update the bargain info -DJB-
void updatebargain(int store_num, int32_t price, int32_t minprice) {
    Store_t *s_ptr = &stores[store_num];

    if (minprice > 9) {
        if (price == minprice) {
            if (s_ptr->good_buy < MAX_SHORT) {
                s_ptr->good_buy++;
            }
        } else {
            if (s_ptr->bad_buy < MAX_SHORT) {
                s_ptr->bad_buy++;
            }
        }
    }
}
