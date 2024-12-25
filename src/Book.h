// Book.h

#pragma once

#ifndef BOOK_H
#define BOOK_H

#include "Board.h"
#include "Def.h"
#include "Types.h"

#define MAX_CHILDREN 32

typedef struct Node {
    MoveItem Move;

    int White;
    int Draw;
    int Black;

    int Total;

    struct Node* Children[MAX_CHILDREN];
} NodeItem; // 284 bytes (aligned 288 bytes)

typedef struct {
    U64 Hash;

    int From;
    int To;

    int Total;
} BookItem; // 20 bytes (aligned 24 bytes)

typedef struct {
    int Count;

    BookItem* Item;
} BookStoreItem;

extern BookStoreItem BookStore;

extern BOOL BookFileLoaded;

void GenerateBook(void);

BOOL LoadBook(const char* BookFileName);

BOOL GetBookMove(const BoardItem* Board, MoveItem* BestMoves);

#endif // !BOOK_H