#include "roomview.h"
#include "ui_roomview.h"

#include <stdexcept>

#include <QGuiApplication>
#include <QCursor>
#include <QScrollBar>

#include "matrix/Room.hpp"
#include "matrix/Member.hpp"
#include "TimelineView.hpp"
#include "WrappingTextEdit.hpp"
#include "RoomMenu.hpp"

QString RoomView::Compare::key(const QString &n) {
  int i = 0;
  while((n[i] == '@') && (i < n.size())) {
    ++i;
  }
  if(i == n.size()) return n.toCaseFolded();
  return QString(n.data() + i, n.size() - i).toCaseFolded();
}

RoomView::RoomView(matrix::Room &room, QWidget *parent)
    : QWidget(parent), ui(new Ui::RoomView), timeline_view_(new TimelineView(room, this)), entry_(new WrappingTextEdit(this)),
      room_(room) {
  ui->setupUi(this);

  auto menu = new RoomMenu(room, this);
  connect(ui->menu_button, &QAbstractButton::clicked, [this, menu](bool) {
      menu->popup(QCursor::pos());
    });

  ui->central_splitter->insertWidget(0, timeline_view_);

  ui->layout->insertWidget(2, entry_);
  setFocusProxy(entry_);
  entry_->installEventFilter(this);

  connect(&room_, &matrix::Room::message, this, &RoomView::message);

  connect(&room_, &matrix::Room::membership_changed, this, &RoomView::membership_changed);
  connect(&room_, &matrix::Room::member_name_changed, this, &RoomView::member_name_changed);

  auto initial_members = room_.state().members();
  for(const auto &member : initial_members) {
    member_list_.insert(std::make_pair(room_.state().member_name(*member), member));
  }

  connect(&room_, &matrix::Room::prev_batch, timeline_view_, &TimelineView::end_batch);

  auto replay_state = room_.initial_state();
  for(const auto &batch : room_.buffer()) {
    timeline_view_->end_batch(batch.prev_batch);
    for(const auto &event : batch.events) {
      replay_state.apply(event);
      append_message(replay_state, event);
    }
  }

  connect(&room_, &matrix::Room::topic_changed, this, &RoomView::topic_changed);
  topic_changed("");

  update_members();
}

RoomView::~RoomView() { delete ui; }

void RoomView::message(const matrix::proto::Event &evt) {
  if(evt.type == "m.room.message") {
    append_message(room_.state(), evt);
  }
}

void RoomView::member_name_changed(const matrix::Member &member, QString old) {
  auto erased = member_list_.erase(old);
  if(!erased) {
    QString msg = "member name changed from unknown name " + old + " to " + room_.state().member_name(member);
    throw std::logic_error(msg.toStdString().c_str());
  }
  member_list_.insert(std::make_pair(room_.state().member_name(member), &member));
  update_members();
}

void RoomView::membership_changed(const matrix::Member &member, matrix::Membership old) {
  (void)old;
  using namespace matrix;
  switch(member.membership()) {
  case Membership::INVITE:
  case Membership::JOIN:
    member_list_.insert(std::make_pair(room_.state().member_name(member), &member));
    break;

  case Membership::LEAVE:
  case Membership::BAN:
    member_list_.erase(room_.state().member_name(member));
    break;
  }
  update_members();
}

void RoomView::update_members() {
  ui->memberlist->clear();
  for(const auto &member : member_list_) {
    auto item = new QListWidgetItem;
    item->setText(member.first);
    item->setToolTip(member.second->id());
    item->setData(Qt::UserRole, QVariant::fromValue(const_cast<void*>(reinterpret_cast<const void*>(member.second))));
    ui->memberlist->addItem(item);
  }
  if(ui->memberlist->count() == 2) {
    ui->memberlist->hide();
  } else {
    auto scrollbar_width = ui->memberlist->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, ui->memberlist);
    auto margins = ui->memberlist->contentsMargins();
    ui->memberlist->setMaximumWidth(ui->memberlist->sizeHintForColumn(0) + scrollbar_width + margins.left() + margins.right());
  }
}

void RoomView::append_message(const matrix::RoomState &state, const matrix::proto::Event &msg) {
  timeline_view_->push_back(state, msg);
}

void RoomView::topic_changed(const QString &old) {
  (void)old;
  ui->topic->setText(room_.state().topic());
}

bool RoomView::eventFilter(QObject *object, QEvent *event) {
  if(event->type() == QEvent::KeyPress) {
    auto modifiers = QGuiApplication::keyboardModifiers();
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    // TODO: Autocomplete
    if(!(modifiers & Qt::ShiftModifier) &&
       (keyEvent->key() == Qt::Key_Return
        || keyEvent->key() == Qt::Key_Enter)) {
      room_.send_message(entry_->toPlainText());
      entry_->clear();
      return true;
    }
    if(keyEvent->key() == Qt::Key_PageUp) {
      timeline_view_->verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);
    }
    if(keyEvent->key() == Qt::Key_PageDown) {
      timeline_view_->verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
    }
  }
  return false;
}
