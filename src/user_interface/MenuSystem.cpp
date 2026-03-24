#include "MenuSystem.h"

MenuSystem::MenuSystem(int visibleItems) 
    : rootNode(nullptr), currentNode(nullptr), cursorIndex(0), scrollOffset(0), maxVisibleItems(visibleItems) {}

void MenuSystem::setRootMenu(MenuNode* root) {
    rootNode = root;
    currentNode = root;
}

void MenuSystem::handleInput(int delta, bool clicked) {
    if (!currentNode) return;

    // Handle navigation
    int itemCount = (int)currentNode->items.size();
    if (itemCount > 0) {
        cursorIndex += delta;
        if (cursorIndex < 0) cursorIndex = itemCount - 1;
        if (cursorIndex >= itemCount) cursorIndex = 0;

        // Handle scroll logic
        if (cursorIndex < scrollOffset) {
            scrollOffset = cursorIndex;
        } else if (cursorIndex >= scrollOffset + maxVisibleItems) {
            scrollOffset = cursorIndex - maxVisibleItems + 1;
        }
    }

    // Handle selection
    if (clicked && itemCount > 0) {
        MenuItem& item = currentNode->items[cursorIndex];
        if (item.type == MENU_TYPE_SUBMENU && item.targetMenu) {
            currentNode = item.targetMenu;
            cursorIndex = 0;
            scrollOffset = 0;
        } else if (item.type == MENU_TYPE_ACTION && item.action) {
            item.action();
        } else if (item.type == MENU_TYPE_BACK && currentNode->parent) {
            currentNode = currentNode->parent;
            cursorIndex = 0; // Or track previous index? Keeping it simple for now.
            scrollOffset = 0;
        }
    }
}

void MenuSystem::reset() {
    currentNode = rootNode;
    cursorIndex = 0;
    scrollOffset = 0;
}
