#pragma once
#include <thread>
#include <vector>
#include <map>
#include <string>
#include <sstream>

#define GT_E_NULLPTR -1
#define GT_E_
/**
 * CS commands(events):
 * hs <suid> <name> [token] [-u uuid]  : c->s handshake
 * ch <suid> <ok?auth?ban?new> [-t <token: for the new user>  -m <message>] s->c response of hs
 * cd <chid> [-n <name> -p <parent channel> -d <description> -c <voice channel count> -b <bitrate>  -n <current online>]    : s->c channel desc
 * rd <suid> [-n <name> -c  <current channel id> -u <uuid> -l:user leave]  : s->c it means user left if name is not provided.
 * j  <chid> : c->s request to join
 * s  <chid> <session id> <cert code> : s->c return session id
 * v  <chid> <session id> <channel state:[0:unavaliable, 1:avaliable]> : s->c voip verification passed
 * l  <chid> : c->s leave channel
 * cc <chid> [-c <voice channel count> -b <bitrate> -s <new session id> -t <cert code>]  : s->c channel voice config change notify
 * ms <chid> <message> : c->s send message
 * mr <chid> <suid> <name> <message> : s->c received message
 */
typedef uint32_t suid_t;
typedef uint32_t chid_t;
typedef uint32_t usid_t;
typedef uint32_t svid_t;
//struct string
 //{
 //	const char* data;
 //	string(const char* s)
 //	{
 //		auto len = strlen(s);
 //		data = new char[len + 1];
 //		memcpy((char*)data, s, len);
 //		((char*)data)[len] = 0;
 //	}
 //	string()
 //	{
 //		data = "";
 //	}
 //	string(const string& s) :string(s.data)
 //	{
 //
 //	}
 //	string(string&& s) noexcept
 //	{
 //		data = s.data;
 //		s.data = nullptr;
 //	}
 //	string& operator+(string& s)
 //	{
 //		
 //	}
 //};
struct RemoteClientDesc
{
	std::string name;
	uint32_t suid = 0;
	uint64_t uuid = 0;
	uint32_t current_chid = 0;
	std::string avatar;
};
struct ChannelDesc
{
	//channel info
	uint32_t chid;
	uint32_t parent;//chid, 0 if it is in the root
	std::string name;
	uint32_t owner = 0;
	uint32_t n_channel = 1;// voice channel count
	uint32_t bitrate;
	uint32_t capacity = 16;
	std::string description;
	std::string privilege;

};
struct ServerDesc
{
	//server info info
	uint64_t usid;
	std::string host;
	std::string name;
	uint32_t n_group;
};
struct client_state
{
	bool man_mute = false;
	bool man_silent = false;
	bool mute = false;
	bool silent = false;

	std::stringstream& cg_mute(std::stringstream& ss)
	{
		ss << " -mute " << (((int)man_mute) | (((int)mute) << 1));
		return ss;
	}
	std::stringstream& cg_silent(std::stringstream& ss)
	{
		ss << " -silent " << (((int)man_silent) | (((int)silent) << 1));
		return ss;
	}

	bool is_mute()
	{
		return man_mute || mute;
	}
	bool is_silent()
	{
		return man_silent || silent;
	}
};
static std::string escape(const char* in, size_t slen)
{
	std::stringstream out;
	int n = 0;
	for (int i = 0; i < slen; i++)
	{
		switch (in[i])
		{
		case ' ':
		case '\t':
		case '\n':
		case '\'':
		case '\"':
			//case '-':
			out << '\\';
			n++;
			break;

		default:
			break;
		}
		out << in[i];
		n++;
	}
	return out.str();
}

static std::string escape(std::string in)
{
	std::stringstream out;
	int slen = in.length();
	int n = 0;
	bool bs = true;
	for (int i = 0; i < slen; i++)
	{
		switch (in[i])
		{
		case ' ':
		case '\t':
			bs = true;
		case '\n':
		case '\'':
		case '\"':
			//case '-':
			out << '\\';
			n++;
			bs = false;
			break;
		//case '-':
		//	if (bs)
		//		out << '\\';
		//	bs = false;
		//	break;
		default:
			bs = false;
			break;
		}
		out << in[i];
		n++;
	}
	return out.str();
}
static std::string esc_quote(std::string in)
{
	std::stringstream out; out << "\'";
	int slen = in.length();
	int n = 0;
	for (int i = 0; i < slen; i++)
	{
		switch (in[i])
		{
		case '\'':
		case '\"':
			out << '\\';
			n++;
			break;

		default:
			break;
		}
		out << in[i];
		n++;
	}
	out << '\'';
	return out.str();
}
static std::string esc_quote(std::string in, size_t slen)
{
	std::stringstream out; out << "\'";
	int n = 0;
	for (int i = 0; i < slen; i++)
	{
		switch (in[i])
		{
		case '\'':
		case '\"':
			out << '\\';
			n++;
			break;

		default:
			break;
		}
		out << in[i];
		n++;
	}
	out << '\'';
	return out.str();
}

class command
{
	friend class command_buffer;
	std::vector<std::string> tokens;
	std::vector<std::string*> args;
	std::map<std::string, std::vector<std::string*>> opts;
	std::string empty;
	void parse()
	{
		bool bOpt = false;
		std::string opt_name = "";
		std::vector<std::string*> opt_val;
		for (auto& i : tokens)
		{
			if (!bOpt && i[0] == '-')
				bOpt = true;
			if (!bOpt)
			{
				args.push_back(&i);
			}
			else
			{
				if (i[0] == '-')
				{
					if (opt_name != "")
					{
						if (opts.count(opt_name))
						{
							opts[opt_name].insert(opts[opt_name].end(), opt_val.begin(), opt_val.end());
						}
						else
						{
							opts[opt_name] = opt_val;
						}
						opt_val.clear();
					}
					opt_name = i;
				}
				else
				{
					opt_val.push_back(&i);
				}
			}

		}
		if (opt_name != "")
		{
			if (opts.count(opt_name))
			{
				opts[opt_name].insert(opts[opt_name].end(), opt_val.begin(), opt_val.end());
			}
			else
			{
				opts[opt_name] = opt_val;
			}
			opt_val.clear();
		}

	}
public:
	bool has_option(std::string opt)// -xx
	{
		return opts.count(opt);
	}
	std::string& at(uint32_t index)//at0 at1 at2 at3 -x xxx -vv vvvv
	{
		if (index > args.size()) return empty;
		return *args[index];

	}
	std::string& token(uint32_t index)
	{
		if (index > tokens.size()) return empty;
		return tokens[index];
	}
	std::string& operator[](uint32_t index)
	{
		return *args[index];
	}
	int n_opt_val(std::string opt)
	{
		if (!has_option(opt))
			return 0;
		return opts[opt].size();
	}
	std::string& operator[](std::string opt)
	{
		return *opts[opt][0];
	}
	std::string& option(std::string opt, int index = 0)//-a xxx xxx xxx
	{
		return *opts[opt][index];
	}
	int n_args()
	{
		return args.size();
	}
	int n_opts()
	{
		return opts.size();
	}
	std::string str()
	{
		std::string s = "";
		for (auto& i : tokens)
			s += escape(i) + " ";
		return s;
	}
	void clear()
	{
		tokens.clear();
		args.clear();
		opts.clear();
	}
	void remove_head()
	{
		if (args.size())
			args.erase(args.begin());
	}

	operator const std::string& () const
	{
		if (args.size())
			return *args[0];
		return empty;
	}
	bool operator== (std::string&& s)
	{
		if (args.size())
			return *args[0] == s;
		return false;
	}
};
class command_buffer
{
	std::stringstream ss;
	bool esc = false;
	int n_cmd = 0;
	int quote = 0;
	char escape(char c)
	{
		switch (c)
		{
		case 'n':
			return '\n';
		case 't':
			return '\t';
		default:
			return c;
		}
	}
public:
	int append(const char* buf, int sz)
	{
		for (int i = 0; i < sz; i++)
		{
			if (buf[i] == '\0')
			{
				sz = i;
				break;
			}
			if (esc)
			{
				esc = false;
				continue;
			}
			if (buf[i] == '\'')
			{
				quote = (quote & ~0b1) | (~quote & 0b1); //bit flip
				continue;
			}
			if (buf[i] == '\"')
			{
				quote = (quote & ~0b10) | (~quote & 0b10);
				continue;
			}

			if (buf[i] == '\\')
			{
				esc = true;
				continue;
			}
			else if (buf[i] == '\n' && !quote)
				n_cmd++;
		}
		ss.write(buf, sz);
		return n_cmd;
	}
	int parse(std::vector<std::string>& out)
	{
		if (!n_cmd) return -1;
		auto s = ss.str();
		ss.str("");
		std::stringstream sstr;
		std::string rs;
		bool esc = false;
		int quote = 0;
		int c = 0;
		int ign = true;
		for (int i = 0; i < s.length(); i++)
		{
			if (esc)
			{
				sstr << escape(s[i]);
				esc = false;
				continue;
			}
			if (s[i] == '\'')
			{
				quote = (quote & ~0b1) | (~quote & 0b1); //bit flip
				ign = false;
				continue;
			}
			if (s[i] == '\"')
			{
				quote = (quote & ~0b10) | (~quote & 0b10);
				ign = false;
				continue;
			}
			if (quote)
			{
				sstr << s[i];
				continue;
			}
			if (s[i] == '\\')
			{
				esc = true;
				continue;
			}
			if (ign && s[i] != '\t' && s[i] != ' ')
			{
				ign = false;
			}
			if (ign) continue;
			if (!quote && (s[i] == ' ' || s[i] == '\n'))
			{
				rs = sstr.str();
				if (!rs.length())
				{
					if (s[i] == '\n')
						n_cmd--;
					if (c)
					{
						ss.write(&s[i + 1], s.size() - i - 1);
						return c;
					}
					continue;
				}
				sstr.str("");
				out.push_back(rs);
				ign = true;
				c++;
				if (s[i] == '\n')
				{
					n_cmd--;
					ss.write(&s[i + 1], s.size() - i - 1);
					return c;
				}
				continue;
			}
			sstr << s[i];
		}

		return out.size();
	}
	int parse(command& out)
	{
		auto r = parse(out.tokens);
		out.parse();
		return r;
	}

};
int edcrypt_voip_pack(char* buf, int sz, uint32_t cert);

inline uint64_t stru64(std::string s)
{
	return std::strtoull(s.c_str(), nullptr, 10);
}

extern bool discard;
