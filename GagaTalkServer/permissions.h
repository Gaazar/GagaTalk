#pragma once
#include <string>

static std::string owner_server_role =
"\
manage\n\
sql\n\
remote.command\n\
remote.command.broadcast\n\
\n\
\n\
report\n\
broadcast\
create.role.server\n\
modify.role.server\n\
delete.role.server\n\
admin.ban\n\
grant.role.server\n\
create.channel\n\
create.channel.permanent\n\
create.channel.password\n\
create.channel.role\n\
create.server.role\n\
modify.channel.quality\n\
modify.channel.name\n\
modify.channel.description\n\
modify.channel.icon\n\
modify.channel.info\n\
modify.channel.role\n\
modify.server.name\n\
modify.server.description\n\
modify.server.icon\n\
modify.server.info\n\
modify.server.role\n\
delete.channel\n\
grant.channel.role\n\
grant.server.role\n\
admin.ban\n\
admin.kick\n\
admin.move\n\
admin.silent\n\
admin.mute\n\
speak\n\
text\n\
text.whisper\n\
role.keygen\n\
join\n\
";
static std::string default_server_role =
"\
report\n\
text.whisper\n\
";
static std::string owner_channel_role =
"\
manage\n\
create.channel\n\
modify.channel.name\n\
modify.channel.description\n\
modify.channel.icon\n\
modify.channel.info\n\
delete.channel\n\
grant.channel.role\n\
speak\n\
text\n\
join\n\
";
static std::string default_channel_role =
"\
speak\n\
text\n\
join\n\
";