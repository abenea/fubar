#pragma once

#include <QLabel>

// One-line label that elides its text to its width. It asks for at most `maxWidth` pixels and
// needs none, so a long text never widens the window; the tooltip holds the full text.
class ElidedLabel : public QLabel {
    Q_OBJECT
public:
    explicit ElidedLabel(int maxWidth, QWidget *parent = nullptr);

    void setFullText(const QString &text);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void elide();

    int maxWidth_;
    QString fullText_;
};
