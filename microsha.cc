#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <vector>
#include <string>
#include <string.h>
#include <iostream>
#include <fstream>
using namespace std;

const string HOMEDIRECTORY = "/Users/apple";
const int HOMEDIR_DEPTH = 2;
const string regularUserSymbol = ">";
const string superUserSymbol = "!";
const char anySymbolMetasymbol = '?';
const char wildcardMetasymbol = '*';
const char outputRedirectSymbol = '>';
const char inputRedirectSymbol = '<';
const int BUF_SIZE = 400;

void parse(string &str, vector<string> &parsedStr) {
	int i = 0;
	while ((i < str.length()) && ((str[i] == ' ') || (str[i] == '\t'))) i++;
	while (i < str.length()) {
		int startIndex = i;
		while ((i < str.length()) && ((str[i] != ' ') && (str[i] != '\t'))) i++;
		parsedStr.push_back(str.substr(startIndex, i - startIndex));
		while ((i < str.length()) && ((str[i] == ' ') || (str[i] == '\t'))) i++;
	}
	return; 
}

int changeDirectory(string &cdPath, string &oldPath, string &newPath, int curDepth) { // Finds new directory path and depth
	if (cdPath.length() == 0) {
		newPath = HOMEDIRECTORY;
		return HOMEDIR_DEPTH;
	}
	int depth = 0;
	if ((cdPath[0] != '~') && (cdPath[0] != '/') && (cdPath[0] != '.') && (cdPath.substr(0,2) != "..")) {
		cdPath = oldPath + "/" + cdPath;
	}
	
	int i = 0;
	if (cdPath[0] == '~') {
		cdPath.erase(0, 1);
		cdPath = HOMEDIRECTORY + "/" + cdPath;
	} 
	if ((cdPath[0] == '.') && ((cdPath.length() == 1) || (cdPath[1] == '/'))) {
		cdPath.erase(0, 1);
		cdPath = oldPath + "/" + cdPath;
	}
	i++;
	while ((i < cdPath.length()) && (cdPath[i] == '/') && (cdPath[i-1] == '/')) {
		cdPath.erase(cdPath.begin() + i);
	}
	while (i < cdPath.length()) {
		while ((i < cdPath.length()) && (cdPath[i] != '/')) i++;
		depth++;
		i++;
		while ((i < cdPath.length()) && (cdPath[i] == '/'))	cdPath.erase(cdPath.begin() + i);
	}
	if (cdPath[cdPath.length() - 1] == '/') cdPath.erase(cdPath.length() - 1, cdPath.length());
	if (cdPath.length() == 0) cdPath = '/';
	
	int j = 2;
	int goBack = 0;
	while (((j < cdPath.length()) && (cdPath.substr(j-2, 3) == "../")) || ((j == cdPath.length()) && (cdPath.substr(j-2, 2) == ".."))) {
		goBack++;
		j += 3;
	}
		
	if (goBack > curDepth) {
		depth = depth - goBack;
		goBack = curDepth;
	} else {
		depth = depth + curDepth - 2*goBack;
	}
	if (depth < 0) depth = 0;
	//cout << cdPath << " goBack = " << goBack << " depth = " << depth << endl;
	
	string oldPathGoBack = oldPath;
	for (int l = 0; l < goBack; l++) {
		int found = oldPathGoBack.rfind("/");
		if (found != string::npos) oldPathGoBack.erase(found);
	}
	if (oldPathGoBack.length() == 0) oldPathGoBack = "/";
	//cout << oldPathGoBack << " j = " << j << endl;
	
	if (goBack > 0) {
		if ((cdPath.length() == j-2) || (cdPath.length() == j-3)) {
			newPath = oldPathGoBack;
		} else if (oldPathGoBack == "/") {
			newPath = "/" + cdPath.substr(j-2, cdPath.length() - j+2);
		} else {
			newPath = oldPathGoBack + "/" + cdPath.substr(j-2, cdPath.length() - j+2);
		}
	} else {
		newPath = cdPath;
	}
	//cout << newPath << " depth = " << depth << endl;
	
	return depth; 
}

void getShortDirectory(string &path, string &shortDir) {
	if (path == "/") {
		shortDir = "/";
	} else {
		int found = path.rfind("/");
		if (found != string::npos) shortDir = path.substr(found+1, path.length() - found);
	}
	return;
}


void redirections(vector<string> &inputVector, int fdlogs) {
	int i = 0;
	bool anythingChanged = false;
	while (i < inputVector.size()) { //input has format "substr1>substr2 substr3", case if there is more than 1 sign '>'
		anythingChanged = false;
		int foundOutputRedirect = inputVector[i].find(outputRedirectSymbol);
		int foundInputRedirect = inputVector[i].find(inputRedirectSymbol);
		
		/*string s = to_string(i);
		write(fdlogs, "i = ", 4);
		write(fdlogs, s.c_str(), s.length());
		write(fdlogs, "\n", 1);*/
		
		if ((foundOutputRedirect != string::npos) || (foundInputRedirect != string::npos)) {
			anythingChanged = true;
			int pos = (foundOutputRedirect == string::npos) ? foundInputRedirect : foundOutputRedirect;
			bool isOutput = (foundOutputRedirect == string::npos) ? false : true;
			int j = i;
			string substr1 = inputVector[j].substr(0, pos);
			string substr2 = inputVector[j].substr(pos+1, inputVector[j].length()-pos-1);
			bool substr3exists = (j == inputVector.size()-1)? false : true;
			string substr3 = "";
			if (substr3exists) substr3 = inputVector[j+1];
			//cout << "i = " << i << ": " << substr1 << " " << substr2 << " " << substr3 << endl;
			if (((substr1 != "0") && (substr1 != "") && (!isOutput)) || ((substr1 != "1") && (substr1 != "2") && (substr1 != "") && (isOutput))) {
				inputVector.insert(inputVector.begin() + j, substr1);
				substr1 = "";
				j++; //making j point to a string with '>' sign
			}
			if (substr1 == "") {
				substr1 = "0";
				if (isOutput) substr1 = "1";
			}  //by default it is 1 (stdout) or 0 (stdin)
			if ((substr2.find(wildcardMetasymbol) != string::npos) || (substr2.find(anySymbolMetasymbol) != string::npos)) {
				cout << substr2 << ": ambiguous redirect" << endl;
				continue;
			}
			if ((substr2 == "") && (!substr3exists)) {
				cout << "syntax error near unexpected token 'newline'" << endl;
				continue;
			}
		
			if (substr2 == "") {
				substr2 = substr3;
				inputVector.erase(inputVector.begin() + j+1);
			}
			inputVector.erase(inputVector.begin()+j);
			
			for (int l = 0; l < inputVector.size(); l++) {
				write(fdlogs, inputVector[l].c_str(), inputVector[l].length());
				write(fdlogs, "\n", 1);
			}
			
			close(stoi(substr1));
			int redirectionTo;
			if (isOutput) {
				redirectionTo = open(substr2.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
			} else {
				redirectionTo = open(substr2.c_str(), O_RDONLY, 0666);
			}
			if (redirectionTo < 0) perror("open");
			break;
		}
		i++;
	}
	for (int l = 0; l < inputVector.size(); l++) {
		write(fdlogs, inputVector[l].c_str(), inputVector[l].length());
		write(fdlogs, "\n", 1);
	}
	if (anythingChanged) redirections(inputVector, fdlogs);
}



class TwoStrings {
public:
	TwoStrings(string &header, string &mask) {
		this->header = header;
		this->mask = mask;
	}
	int prefixFunctionWithExtraSymbol(string &str, char extraSymbol, int stopper) { //stop when found ret[i] = stopper, return it's position
		int L = str.length();
		
		vector<int> ret(L, 0);
		for (int i = 1; i < L; i++) {
			int j = ret[i-1];
			if (str[i] == '#') {
				ret[i] = 0;
				continue;
			}
			while ((j > 0) && (str[i] != str[j]) && (str[j] != extraSymbol) && (str[i] != extraSymbol)) {
				j = ret[j-1];
			}
			if ((str[i] == str[j]) || (str[j] == extraSymbol) || (str[i] == extraSymbol)) {
				j++;
			}
			ret[i] = j;
			//cout << "ret[" << i << "] = " << j << "\n";
			if ((j == stopper) && (i > stopper)) return (i - stopper);
		}
		return -1;
	}
	
	int findSubstrInHeader(string &substr, int pos, char extraSymbol) { 
			//returns the index of the next symbol after first occurence of substr, header.length() otherwise
		string helperString = header;
		helperString.erase(0, pos);
		helperString = substr + "#" + helperString;
		//cout << substr << " : " << pos << " : " << substr.length() << " : " << helperString << "\n";
		
		int next = prefixFunctionWithExtraSymbol(helperString, extraSymbol, substr.length());
		//cout << "next = " << next << "\n";
		if (substr.length() == 0) next = 0;
		return (next == -1)?(-1):(next+pos);
	}
	
	bool match() { //true if the mask is correct, false otherwise
		char wildcard = wildcardMetasymbol;
		char anySymbol = anySymbolMetasymbol;
		int headerPos = 0, maskPos = 0; // Current positions in strings
		int headerLength = header.length();
		int maskLength = mask.length();
		
		int lastWildcardPos = -1;
		int lastBetweenWildcardsLength = -1;
		
		while (1) {
			//cout << "h = " << headerPos << ", m = " << maskPos << "\n";
			if ((headerPos == headerLength) && (maskPos == maskLength)) return true;
			if ((maskPos == maskLength) && (lastWildcardPos == -1)) {
				return false;
			} else if (maskPos == maskLength) {
				maskPos = lastWildcardPos;
				headerPos = headerPos - lastBetweenWildcardsLength + 1;
				continue;
			}
			
			if (mask[maskPos] == wildcard) {
				string betweenWildcards;
				lastWildcardPos = maskPos;
				int nextWildcard;
				if (maskPos == 0) {
					betweenWildcards = "";
					nextWildcard = 1;
				} else {
					betweenWildcards = mask;
					nextWildcard = mask.find(wildcard, maskPos+1);
					if (nextWildcard == string::npos) {
						betweenWildcards.erase(0, maskPos+1);
						nextWildcard = maskLength;
					} else {
						betweenWildcards.erase(nextWildcard, maskLength);
						betweenWildcards.erase(0, maskPos+1);
					}
				}
				//cout << "bwc: " << betweenWildcards << "\n";
				lastBetweenWildcardsLength = betweenWildcards.length();
				headerPos = findSubstrInHeader(betweenWildcards, headerPos, anySymbol);
				if (headerPos == -1) return false;
				maskPos = nextWildcard;
				continue;
			}
			
			if (headerPos == headerLength) return false;
			
			if ((header[headerPos] == mask[maskPos]) || (mask[maskPos] == anySymbol)) {
				headerPos++;
				maskPos++;
				continue;
			}
			if (lastWildcardPos != -1) {
				headerPos++;
				continue;
			}
			return false;
		}
	}
	
private:
	string header;
	string mask;
};


void complexFork(const vector< vector<char *> > &charVector, int commandNumber, int numberOfCommands, int parentPid, int fdlogs) {
	if (commandNumber == numberOfCommands) { // process running 1st program
		if (execvp(charVector[numberOfCommands - commandNumber][0], &charVector[numberOfCommands - commandNumber][0]) < 0) { //1st command
			perror("execvp");
			_exit(0);
		}
		return;		
	}
	int fd[2];
	if (pipe(fd) < 0) perror("pipe");
	pid_t newpid = fork();
	if (newpid < 0) perror("fork");
	if (newpid == 0) {
		//cout << "cN = " << commandNumber+1 << ": pid = " << newpid << endl;
		close(1);
		int fd_1 = dup(fd[1]);
		complexFork(charVector, commandNumber+1, numberOfCommands, newpid, fdlogs);
	} else {
		close(0);
		close(fd[1]);
		int fd_0 = dup(fd[0]);
		if (execvp(charVector[numberOfCommands - commandNumber][0], &charVector[numberOfCommands - commandNumber][0]) < 0) {
			perror("execvp");
			_exit(0);
		}
	}
	return;
}


int main() {
	
	/* Redirecting stderr to file */
	close(2);
	int fderr = open("errlogs", O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (fderr < 0) perror("open");
	int fdlogs = open("logs", O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (fdlogs < 0) perror("open");
	
	/* Preparing the directory */
	struct stat st;
	if (stat(HOMEDIRECTORY.c_str(), &st) < 0) { // initializing stat structure
		perror(HOMEDIRECTORY.c_str());
		return 0;
	}
	string currentDirectory = HOMEDIRECTORY;
	string currentDirectoryShort;
	getShortDirectory(currentDirectory, currentDirectoryShort);
	int currentDirectoryDepth = HOMEDIR_DEPTH;
	DIR *curDir_dir = opendir(currentDirectory.c_str());
	if (chdir(currentDirectory.c_str()) < 0) perror("chdir");
	
	/* Determining user priveleges */
	string userSymbol = regularUserSymbol;
	uid_t uid = getuid();
	if (uid == 0) userSymbol = superUserSymbol;
	
	/* Reading input line */
	string inputString;
	
	for (;;) {
		vector<string> inputVector;
		cout << currentDirectoryShort << " " << userSymbol << " ";
		getline(cin, inputString);
		parse(inputString, inputVector);
		
		/* Exit */
		if (inputVector.size() > 0 && inputVector[0] == "exit") {
			return 0;
		}		
		
		/* Change directory */		
		if (inputVector.size() > 0 && inputVector[0] == "cd") {
			vector<string> pathVector;
			string cdPath = (inputVector.size() > 1) ? inputVector[1] : "~";
			string newPath; // new directory of successful
			int newDepth = changeDirectory(cdPath, currentDirectory, newPath, currentDirectoryDepth);
			if (stat(newPath.c_str(), &st) < 0) {
				perror("stat");
				cout << newPath << ": Not a directory." << endl;
			} else { //success
				currentDirectory = newPath;
				currentDirectoryDepth = newDepth;
				getShortDirectory(currentDirectory, currentDirectoryShort);
				curDir_dir = opendir(currentDirectory.c_str());
				if (chdir(currentDirectory.c_str()) < 0) perror("chdir");
			}
			continue;
		}
		
		/* Print working directory */		
		if (inputVector.size() > 0 && inputVector[0] == "pwd") {
			cout << currentDirectory << endl;
			continue;
		}
		
		
		/* Input/Output redirection */
		int pid = fork();
		if (pid < 0) perror("fork");
		if (pid == 0) { // Only done by a child
			redirections(inputVector, fdlogs);
		} else {
			wait(0);
		}
		
		
		/* Time */
		if (pid == 0) {
			if ((inputVector.size() > 0) && (inputVector[0] == "time")) {
				struct rusage r_usage;
				struct timeval sys_start, sys_end, u_start, u_end;
				vector<char *> placeholder;
				for (int j = 1; j < inputVector.size(); j++) {
					placeholder.push_back((char *)inputVector[j].c_str());
				}
				placeholder.push_back(NULL);
				getrusage(RUSAGE_SELF, &r_usage);
				sys_start = r_usage.ru_stime;
				u_start = r_usage.ru_utime;
				pid_t newpid = fork();
				if (newpid < 0) perror("fork");
				if (newpid == 0) {
					if (execvp(placeholder[0], &placeholder[0]) < 0) {
						perror("execvp");
						_exit(0);
					}
				} else {
					wait(0);
					getrusage(RUSAGE_SELF, &r_usage);
					sys_end = r_usage.ru_stime;
					u_end = r_usage.ru_utime;
					long int sys_sec = sys_end.tv_sec - sys_start.tv_sec;
					long int sys_usec = sys_end.tv_usec - sys_start.tv_usec;
					if (sys_usec < 0) {
						sys_usec = 1000 - sys_usec;
						sys_sec++;
					}
					long int u_sec = u_end.tv_sec - u_start.tv_sec;
					long int u_usec = u_end.tv_usec - u_start.tv_usec;
					if (u_usec < 0) {
						u_usec = 1000 - u_usec;
						u_sec++;
					}
					printf("sys     %ld.%lds\n", sys_sec, sys_usec);
					printf("user    %ld.%lds\n", u_sec, u_usec);
				}
			}
		} else {
			wait(0);
		}
		
		/* Metasymbols * and ? */
		if (pid == 0) {
			for (int i = 0; i < inputVector.size(); i++) {
				int foundWildcard = inputVector[i].find(wildcardMetasymbol);
				int foundAnySymbol = inputVector[i].find(anySymbolMetasymbol);
				if ((foundWildcard != string::npos) || (foundAnySymbol != string::npos)) {
					string pattern = inputVector[i];
					inputVector.erase(inputVector.begin() + i);
					curDir_dir = opendir(currentDirectory.c_str());
					bool flag = false;
					for (dirent *de = readdir(curDir_dir); de != NULL; de = readdir(curDir_dir)) {
						string de_string = string(de->d_name);
						struct stat st;
						stat(de_string.c_str(), &st);
						if (de_string == ".") continue;
						if (de_string == "..") continue;
						if (!S_ISDIR(st.st_mode)) {
							TwoStrings toMatch(de_string, pattern);
							//int z = (toMatch.match()) ? 1 : 0;
							//cout << de_string << " " << pattern << " " << z << " " << endl;
							if (toMatch.match()) {
								inputVector.insert(inputVector.begin() + i, de_string);
								flag = true;
							} 
						}
					}
					if (!flag) i--;
					//inputVector[i] = substitute; 
					//cout << "sub: " << substitute << endl;
				}
			}
		} else {
			wait(0);
		}
		
		/* System calls with pipeline */	
		
		if (pid == 0) { // Child
			
			if (inputVector.size() == 0) _exit(0);
			if (inputVector[0] == "|") _exit(0);
			if (inputVector[inputVector.size()-1] == "|") _exit(0);
			int i = 0;
			int lastIndex = -1;
			int numberOfCommands = 1;
			vector< vector<char *> > charVector;
			while (i < inputVector.size()) {
				if ((inputVector[i] != "|") && (i+1 < inputVector.size())) {
					i++;
					continue;
				}
				if (i+1 == inputVector.size()) i++;
				vector<char *> placeholder;
				for (int j = lastIndex+1; j < i; j++) { // Dividing input vector in different commands that will be piped
					placeholder.push_back((char *)inputVector[j].c_str());
				}
				lastIndex = i;
				placeholder.push_back(NULL);
				charVector.push_back(placeholder);
				
				/*for (int l = 0; l < charVector[numberOfCommands-1].size() - 1; l++) {
					cout << charVector[numberOfCommands-1][l] << endl;
				}*/
				
				if (i != inputVector.size()) numberOfCommands++;
				i++;
			}
			if (numberOfCommands > 0) complexFork(charVector, 1, numberOfCommands, pid, fdlogs);
			
			/*if (execvp(charVector[0][0], &charVector[0][0]) < 0) {
				perror("execvp");
				_exit(0);
			}*/
		} else { // Parent
			wait(0);
		}
	}

	return 0;
}
