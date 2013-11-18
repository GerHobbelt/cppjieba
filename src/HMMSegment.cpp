#include <assert.h>
#include "HMMSegment.h"

namespace CppJieba
{
    HMMSegment::HMMSegment()
    {
        memset(_startProb, 0, sizeof(_startProb));
        memset(_transProb, 0, sizeof(_transProb));
        _statMap[0] = 'B';
        _statMap[1] = 'E';
        _statMap[2] = 'M';
        _statMap[3] = 'S';
        _emitProbVec.push_back(&_emitProbB);
        _emitProbVec.push_back(&_emitProbE);
        _emitProbVec.push_back(&_emitProbM);
        _emitProbVec.push_back(&_emitProbS);
    }
    
    HMMSegment::~HMMSegment()
    {
        dispose();
    }

    bool HMMSegment::init(const char* const modelPath)
    {
        return _setInitFlag(_loadModel(modelPath));
    }
    
    bool HMMSegment::dispose()
    {
        _setInitFlag(false);
        return true;
    }

    bool HMMSegment::_loadModel(const char* const filePath)
    {
        LogInfo("loadModel [%s] start ...", filePath);
        ifstream ifile(filePath);
        string line;
        vector<string> tmp;
        vector<string> tmp2;
        //load _startProb
        if(!_getLine(ifile, line))
        {
            return false;
        }
        splitStr(line, tmp, " ");
        if(tmp.size() != STATUS_SUM)
        {
            LogError("start_p illegal");
            return false;
        }
        for(uint j = 0; j< tmp.size(); j++)
        {
            _startProb[j] = atof(tmp[j].c_str());
            //cout<<_startProb[j]<<endl;
        }

        //load _transProb
        for(uint i = 0; i < STATUS_SUM; i++)
        {
            if(!_getLine(ifile, line))
            {
                return false;
            }
            splitStr(line, tmp, " ");
            if(tmp.size() != STATUS_SUM)
            {
                LogError("trans_p illegal");
                return false;
            }
            for(uint j =0; j < STATUS_SUM; j++)
            {
                _transProb[i][j] = atof(tmp[j].c_str());
                //cout<<_transProb[i][j]<<endl;
            }
        }

        //load _emitProbB
        if(!_getLine(ifile, line) || !_loadEmitProb(line, _emitProbB))
        {
            return false;
        }
        
        //load _emitProbE
        if(!_getLine(ifile, line) || !_loadEmitProb(line, _emitProbE))
        {
            return false;
        }
        
        //load _emitProbM
        if(!_getLine(ifile, line) || !_loadEmitProb(line, _emitProbM))
        {
            return false;
        }

        //load _emitProbS
        if(!_getLine(ifile, line) || !_loadEmitProb(line, _emitProbS))
        {
            return false;
        }

        LogInfo("loadModel [%s] end.", filePath);

        return true;
    }

    bool HMMSegment::cut(Unicode::const_iterator begin, Unicode::const_iterator end, vector<Unicode>& res)const
    {
        /*if(!_getInitFlag()) TODO use Decorator
        {
            LogError("not inited.");
            return false;
        }*/
		assert(_getInitFlag());
        vector<uint> status; 
        if(!_viterbi(begin, end, status))
        {
            LogError("_viterbi failed.");
            return false;
        }

        Unicode::const_iterator left = begin;
        Unicode::const_iterator right;
        for(uint i =0; i< status.size(); i++)
        {
            if(status[i] % 2) //if(E == status[i] || S == status[i])
            {
                right = begin + i + 1;
                res.push_back(Unicode(left, right));
                left = right;
            }
        }
        return true;
    }

    bool HMMSegment::cut(const string& str, vector<string>& res)const
    {
        return SegmentBase::cut(str, res);
    }

    bool HMMSegment::cut(Unicode::const_iterator begin, Unicode::const_iterator end, vector<string>& res) const
    {
        /*if(!_getInitFlag()) TODO use Decorator 18/11/13 09:46:26
        {
            LogError("not inited.");
            return false;
        }*/
		assert(_getInitFlag());
        if(begin == end)
        {
            return false;
        }
        vector<Unicode> words;
        if(!cut(begin, end, words))
        {
            return false;
        }
        string tmp;
        for(uint i = 0; i < words.size(); i++)
        {
            if(TransCode::encode(words[i], tmp))
            {
                res.push_back(tmp);
            }
        }
        return true;
    }

    bool HMMSegment::_viterbi(Unicode::const_iterator begin, Unicode::const_iterator end, vector<uint>& status)const
    {
        if(begin == end)
        {
            return false;
        }

        size_t Y = STATUS_SUM;
        size_t X = end - begin;
        size_t XYSize = X * Y;
        int * path;
        double * weight;
        uint now, old, stat;
        double tmp, endE, endS;
        
        try
        {
            path = new int [XYSize];
            weight = new double [XYSize];
        }
        catch(const std::bad_alloc&)
        {
            LogError("bad_alloc");
            return false;
        }
        if(NULL == path || NULL == weight)
        {
            LogError("bad_alloc");
            return false;
        }

        //start
        for(uint y = 0; y < Y; y++)
        {
            weight[0 + y * X] = _startProb[y] + _getEmitProb(_emitProbVec[y], *begin, MIN_DOUBLE);
            path[0 + y * X] = -1;
        }
        //process
        //for(; begin != end; begin++)
        for(uint x = 1; x < X; x++)
        {
            for(uint y = 0; y < Y; y++)
            {
                now = x + y*X;
                weight[now] = MIN_DOUBLE;
                path[now] = E; // warning
                for(uint preY = 0; preY < Y; preY++)
                {
                    old = x - 1 + preY * X;
                    tmp = weight[old] + _transProb[preY][y] + _getEmitProb(_emitProbVec[y], *(begin+x), MIN_DOUBLE);
                    if(tmp > weight[now])
                    {
                        weight[now] = tmp;
                        path[now] = preY;
                    }
                }
            }
        }

        endE = weight[X-1+E*X];
        endS = weight[X-1+S*X];
        stat = 0;
        if(endE > endS)
        {
            stat = E;
        }
        else
        {
            stat = S;
        }
        
        status.assign(X, 0);
        for(int x = X -1 ; x >= 0; x--)
        {
            status[x] = stat;
            stat = path[x + stat*X];
        }

        delete [] path;
        delete [] weight;
        return true;
    }

    bool HMMSegment::_getLine(ifstream& ifile, string& line)
    {
        while(getline(ifile, line))
        {
            trim(line);
            if(line.empty())
            {
                continue;
            }
            if(strStartsWith(line, "#"))
            {
                continue;
            }
            return true;
        }
        return false;
    }

    bool HMMSegment::_loadEmitProb(const string& line, EmitProbMap& mp)
    {
        if(line.empty())
        {
            return false;
        }
        vector<string> tmp, tmp2;
        uint16_t unico = 0;
        splitStr(line, tmp, ",");
        for(uint i = 0; i < tmp.size(); i++)
        {
            splitStr(tmp[i], tmp2, ":");
            if(2 != tmp2.size())
            {
                LogError("_emitProb illegal.");
                return false;
            }
            if(!_decodeOne(tmp2[0], unico))
            {
                LogError("TransCode failed.");
                return false;
            }
            mp[unico] = atof(tmp2[1].c_str());
        }
        return true;
    }

    bool HMMSegment::_decodeOne(const string& str, uint16_t& res)
    {
        Unicode ui16;
        if(!TransCode::decode(str, ui16) || ui16.size() != 1)
        {
            return false;
        }
        res = ui16[0];
        return true;
    }

    double HMMSegment::_getEmitProb(const EmitProbMap* ptMp, uint16_t key, double defVal)const
    {
        EmitProbMap::const_iterator cit = ptMp->find(key);
        if(cit == ptMp->end())
        {
            return defVal;
        }
        return cit->second;
        
    }
}


#ifdef HMMSEGMENT_UT
using namespace CppJieba;


size_t add(size_t a, size_t b)
{
    return a*b;
}
int main()
{
    TransCode::setUtf8Enc();
    HMMSegment hmm;
    hmm.loadModel("../dicts/hmm_model.utf8");
    vector<string> res;
    hmm.cut("小明硕士毕业于北邮网络研究院。。.", res);
    cout<<joinStr(res, "/")<<endl;
    
    
    return 0;
}

#endif
