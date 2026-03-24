#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <Arduino.h>
#include <vector>
#include <functional>

enum MenuItemType {
    MENU_TYPE_SUBMENU,
    MENU_TYPE_ACTION,
    MENU_TYPE_BACK
};

typedef std::function<void()> MenuAction;

class MenuNode;

class MenuItem {
public:
    String label;
    MenuItemType type;
    MenuNode* targetMenu;
    MenuAction action;

    MenuItem(String l, MenuItemType t, MenuNode* target = nullptr, MenuAction act = nullptr)
        : label(l), type(t), targetMenu(target), action(act) {}
};

class MenuNode {
public:
    String title;
    MenuNode* parent;
    std::vector<MenuItem> items;
    
    MenuNode(String t, MenuNode* p = nullptr) : title(t), parent(p) {}
    
    void addItem(const MenuItem& item) {
        items.push_back(item);
    }
};

class MenuSystem {
private:
    MenuNode* rootNode;
    MenuNode* currentNode;
    int cursorIndex;
    int scrollOffset;
    int maxVisibleItems;

public:
    MenuSystem(int visibleItems = 4);
    void setRootMenu(MenuNode* root);
    void handleInput(int delta, bool clicked);
    
    MenuNode* getCurrentNode() const { return currentNode; }
    int getCursorIndex() const { return cursorIndex; }
    int getScrollOffset() const { return scrollOffset; }
    
    void reset();
};

#endif
