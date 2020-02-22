# Coding Conventions

This document describes some of the recommended coding conventions that should be followed in KWin.

For KWin, it is recommended to follow the KDE Frameworks Coding Style.


## `auto` Keyword

Optionally, you can use the `auto` keyword in the following cases. If in doubt, for example if using
`auto` could make the code less readable, do not use `auto`. Keep in mind that code is read much more
often than written.

* When it avoids repetition of a type in the same statement.

  ```
  auto something = new MyCustomType;
  auto keyEvent = static_cast<QKeyEvent *>(event);
  auto myList = QStringList({ "FooThing",  "BarThing" });
  ```

* When assigning iterator types.

  ```
  auto it = myList.const_iterator();
  ```


## `QRect::right()` and `QRect::bottom()`

For historical reasons, the `QRect::right()` and `QRect::bottom()` functions deviate from the true
bottom-right corner of the rectangle. Note that this is not the case for the `QRectF` class.

As a general rule, avoid using `QRect::right()` and `QRect::bottom()` as well methods that operate
on them. There are exceptions, though.

Exception 1: you can use `QRect::moveRight()` and `QRect::moveBottom()` to snap a `QRect` to
another `QRect` as long as the corresponding borders match, for example

```
// Ok
rect.moveRight(anotherRect.right());
rect.moveBottom(anotherRect.bottom());
rect.moveBottomRight(anotherRect.bottomRight());

// Bad
rect.moveRight(anotherRect.left() - 1); // must be rect.moveLeft(anotherRect.left() - rect.width());
rect.moveBottom(anotherRect.top() - 1); // must be rect.moveTop(anotherRect.top() - rect.height());
rect.moveBottomRight(anotherRect.topLeft() - QPoint(1, 1));
```

Exception 2: you can use `QRect::setRight()` and `QRect::setBottom()` to clip a `QRect` by another
`QRect` as long as the corresponding borders match, for example

```
// Ok
rect.setRight(anotherRect.right());
rect.setBottom(anotherRect.bottom());
rect.setBottomRight(anotherRect.bottomRight());

// Bad
rect.setRight(anotherRect.left());
rect.setBottom(anotherRect.top());
rect.setBottomRight(anotherRect.topLeft());
```

Exception 3: you can use `QRect::right()` and `QRect::bottom()` in conditional statements as long
as the compared borders are the same, for example

```
// Ok
if (rect.right() > anotherRect.right()) {
    return;
}
if (rect.bottom() > anotherRect.bottom()) {
    return;
}

// Bad
if (rect.right() > anotherRect.left()) {
    return;
}
if (rect.bottom() > anotherRect.top()) {
    return;
}
```
