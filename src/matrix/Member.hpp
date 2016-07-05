#ifndef NATIVE_CHAT_MATRIX_MEMBER_H_
#define NATIVE_CHAT_MATRIX_MEMBER_H_

#include <experimental/optional>

#include <QString>
#include <QUrl>

namespace matrix {

class Room;

namespace proto {
struct Event;
}

enum class Membership {
  INVITE, JOIN, LEAVE, BAN
};

std::experimental::optional<Membership> parse_membership(const QString &m);

class Member {
public:
  Member(QString id) : id_(id) {}

  const QString &id() const { return id_; }
  const QString &display_name() const { return display_name_; }
  const QUrl &avatar_url() const { return avatar_url_; }
  Membership membership() const { return membership_; }

  void dispatch(const proto::Event &);

private:
  const QString id_;
  QString display_name_;  // Optional
  QUrl avatar_url_;       // Optional
  Membership membership_ = Membership::LEAVE;
};

}

#endif
