#ifndef PETSC_HPP_
#define PETSC_HPP_

#if PETSC_VERSION_LT(3,7,0)
#define FFPetscOptionsInsert(a,b,c) PetscOptionsInsert(a,b,c)
#else
#define FFPetscOptionsInsert(a,b,c) PetscOptionsInsert(NULL,a,b,c)
#endif

#if PETSC_VERSION_LT(3,6,0)
#define MatCreateVecs MatGetVecs
#endif

#define HPDDM_SCHWARZ 0
#define HPDDM_FETI    0
#define HPDDM_BDD     0
#define HPDDM_PETSC   1

#include "common.hpp"

namespace PETSc {
template<class HpddmType>
class DistributedCSR {
    public:
        HpddmType*                             _A;
        Mat                                _petsc;
        std::vector<Mat>*                     _vS;
        VecScatter                       _scatter;
        KSP                                  _ksp;
        HPDDM::Subdomain<PetscScalar>** _exchange;
        unsigned int*                        _num;
        unsigned int                       _first;
        unsigned int                        _last;
        unsigned int*                       _cnum;
        unsigned int                      _cfirst;
        unsigned int                       _clast;
        DistributedCSR() : _A(), _petsc(), _vS(), _ksp(), _exchange(), _num(), _first(), _last(), _cnum(), _cfirst(), _clast() { }
        ~DistributedCSR() {
            dtor();
        }
        void dtor() {
            if(_petsc) {
                MatType type;
                PetscBool isType;
                MatGetType(_petsc, &type);
                PetscStrcmp(type, MATNEST, &isType);
                if(isType) {
                    Mat** mat;
                    PetscInt M, N;
                    MatNestGetSubMats(_petsc, &M, &N, &mat);
                    for(PetscInt i = 0; i < M; ++i) {
                        for(PetscInt j = 0; j < N; ++j) {
                            if(mat[i][j]) {
                                MatGetType(mat[i][j], &type);
                                PetscStrcmp(type, MATTRANSPOSEMAT, &isType);
                                if(isType) {
                                    Mat B = mat[i][j];
                                    MatDestroy(&B);
                                }
                                else {
                                    PetscStrcmp(type, MATMPIDENSE, &isType);
                                    if(isType) {
                                        Mat B = mat[i][j];
                                        MatDestroy(&B);
                                    }
                                    else if(i == j) {
                                        PetscBool assembled;
                                        MatAssembled(mat[i][j], &assembled);
                                        if(assembled) {
                                            MatInfo info;
                                            MatGetInfo(mat[i][j], MAT_GLOBAL_SUM, &info);
                                            if(std::abs(info.nz_used) < 1.0e-12) {
                                                Mat B = mat[i][j];
                                                MatDestroy(&B);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                MatDestroy(&_petsc);
            }
            if(_vS) {
                for(int i = 0; i < _vS->size(); ++i)
                    MatDestroy(&(*_vS)[i]);
                delete _vS;
                _vS = nullptr;
            }
            if(_ksp)
                KSPDestroy(&_ksp);
            if(_exchange) {
                _exchange[0]->clearBuffer();
                delete _exchange[0];
                if(_exchange[1]) {
                    _exchange[1]->clearBuffer();
                    delete _exchange[1];
                }
                delete [] _exchange;
                _exchange = nullptr;
            }
            if(_A) {
                if(!std::is_same<HpddmType, HpSchwarz<PetscScalar>>::value)
                    VecScatterDestroy(&_scatter);
                else
                    _A->clearBuffer();
                delete _A;
                _A = nullptr;
            }
            delete [] _num;
            _num = nullptr;
        }
};

template<class HpddmType, typename std::enable_if<std::is_same<HpddmType, HpSchwarz<PetscScalar>>::value>::type* = nullptr>
void globalMapping(HpddmType* const& A, unsigned int*& num, unsigned int& start, unsigned int& end, unsigned int& global, unsigned int* const list) {
    num = new unsigned int[A->getMatrix()->_n];
    A->template globalMapping<'C'>(num, num + A->getMatrix()->_n, start, end, global, A->getScaling(), list);
}
template<class HpddmType, typename std::enable_if<!std::is_same<HpddmType, HpSchwarz<PetscScalar>>::value>::type* = nullptr>
void globalMapping(HpddmType* const& A, unsigned int* const& num, unsigned int& start, unsigned int& end, unsigned int& global, unsigned int* const list) { }
template<class Type, class Tab, typename std::enable_if<!std::is_same<Tab, PETSc::DistributedCSR<HpSchwarz<PetscScalar>>>::value>::type* = nullptr>
void setVectorSchur(Type* ptA, KN<Tab>* const& mT, KN<double>* const& pL) {
    int *is, *js;
    PetscScalar *s;
    unsigned int* re = new unsigned int[pL->n];
    unsigned int nbSchur = 1;
    for(int i = 0; i < pL->n; ++i)
        re[i] = std::abs((*pL)[i]) > 1.0e-12 ? nbSchur++ : 0;
    nbSchur--;
    unsigned int* num;
    unsigned int start, end, global;
    ptA->_A->clearBuffer();
    globalMapping(ptA->_A, num, start, end, global, re);
    delete [] re;
    re = new unsigned int[2 * nbSchur];
    unsigned int* numSchur = re + nbSchur;
    for(int i = 0, j = 0; i < pL->n; ++i) {
        if(std::abs((*pL)[i]) > 1.0e-12) {
            *numSchur++ = num[i];
            re[std::lround((*pL)[i]) - 1] = j++;
        }
    }
    numSchur -= nbSchur;
    delete [] num;
    for(int k = 0; k < mT->n; ++k) {
        MatriceMorse<PetscScalar>* mS = (mT->operator[](k)).A ? static_cast<MatriceMorse<PetscScalar>*>(&(*(mT->operator[](k)).A)) : nullptr;
        int n = mS ? mS->n : 0;
        std::vector<std::vector<std::pair<int, PetscScalar>>> tmp(n);
        if(mS)
            mS->CSR();
        int nnz = mS ? mS->nnz : 0;
        for(int i = 0; i < n; ++i) {
            unsigned int row = re[i];
            tmp[row].reserve(mS->p[i + 1] - mS->p[i]);
            for(int j = mS->p[i]; j < mS->p[i + 1]; ++j)
                tmp[row].emplace_back(re[mS->j[j]], mS->aij[j]);
            std::sort(tmp[row].begin(), tmp[row].end(), [](const std::pair<int, PetscScalar>& lhs, const std::pair<int, PetscScalar>& rhs) { return lhs.first < rhs.first; });
        }
        is = new int[n + 1];
        js = new int[nnz];
        s = new PetscScalar[nnz];
        is[0] = 0;
        for(int i = 0; i < n; ++i) {
            for(int j = 0; j < tmp[i].size(); ++j) {
                *js++ = tmp[i][j].first;
                *s++ = tmp[i][j].second;
            }
            is[i + 1] = is[i] + tmp[i].size();
        }
        js -= nnz;
        s -= nnz;
        int* ia, *ja;
        PetscScalar* c;
        ia = ja = nullptr;
        c = nullptr;
        HPDDM::MatrixCSR<PetscScalar>* dN = new_HPDDM_MatrixCSR<PetscScalar>(mS,true,s,is,js);//->n, mS->m, mS->nbcoef, s, is, js, mS->symetrique, true);
        bool free = dN ? ptA->_A->HPDDM::template Subdomain<PetscScalar>::distributedCSR(numSchur, start, end, ia, ja, c, dN) : false;
        MatCreate(PETSC_COMM_WORLD, &(*ptA->_vS)[k]);
        MatSetSizes((*ptA->_vS)[k], end - start, end - start, global, global);
        MatSetType((*ptA->_vS)[k], MATMPIAIJ);
        if(!ia && !ja && !c)
            ia = new int[2]();
        MatMPIAIJSetPreallocationCSR((*ptA->_vS)[k], reinterpret_cast<PetscInt*>(ia), reinterpret_cast<PetscInt*>(ja), c);
        MatSetOption((*ptA->_vS)[k], MAT_NO_OFF_PROC_ENTRIES, PETSC_TRUE);
        if(free) {
            delete [] ia;
            delete [] ja;
            delete [] c;
        }
        else if(!ja && !c)
            delete [] ia;
        delete dN;
    }
    delete [] re;
    ptA->_A->setBuffer();
}
template<class Type, class Tab, typename std::enable_if<std::is_same<Tab, PETSc::DistributedCSR<HpSchwarz<PetscScalar>>>::value>::type* = nullptr>
void setVectorSchur(Type* ptA, KN<Tab>* const& mT, KN<double>* const& pL) {
    assert(pL == nullptr);
    for(int k = 0; k < mT->n; ++k) {
        (*ptA->_vS)[k] = mT->operator[](k)._petsc;
        PetscObjectReference((PetscObject)mT->operator[](k)._petsc);
    }
}
template<class Type, class Tab>
void setFieldSplitPC(Type* ptA, KSP ksp, KN<double>* const& fields, KN<String>* const& names, KN<Tab>* const& mT, KN<double>* const& pL = nullptr) {
    if(fields) {
        PC pc;
        KSPGetPC(ksp, &pc);
        PCSetType(pc, PCFIELDSPLIT);
        PetscInt first = ptA->_first;
        PetscInt last = ptA->_last;
        if(!ptA->_num) {
            Mat A;
            KSPGetOperators(ksp, &A, NULL);
            MatGetOwnershipRange(A, &first, &last);
        }
        unsigned short* local = new unsigned short[fields->n + last - first]();
        for(int i = 0; i < fields->n; ++i)
            local[i] = std::lround(fields->operator[](i));
        unsigned short nb = fields->n > 0 ? *std::max_element(local, local + fields->n) : 0;
        MPI_Allreduce(MPI_IN_PLACE, &nb, 1, MPI_UNSIGNED_SHORT, MPI_MAX, PETSC_COMM_WORLD);
        local += fields->n;
        if(fields->n) {
            if(ptA->_num)
                HPDDM::Subdomain<PetscScalar>::template distributedVec<0>(ptA->_num, first, last, local - fields->n, local, fields->n);
            else
                std::copy_n(local - fields->n, fields->n, local);
        }
        unsigned long* counts = new unsigned long[nb]();
        unsigned int remove = 0;
        for(unsigned int i = 0; i < last - first; ++i) {
            if(local[i])
                ++counts[local[i] - 1];
            else
                ++remove;
        }
        unsigned int firstFS = 0;
        if(ptA->_ksp != ksp) {
            MPI_Exscan(&remove, &firstFS, 1, MPI_UNSIGNED, MPI_SUM, PETSC_COMM_WORLD);
            if(mpirank == 0)
                firstFS = 0;
        }
        PetscInt* idx = new PetscInt[*std::max_element(counts, counts + nb)];
        for(unsigned short j = 0; j < nb; ++j) {
            IS is;
            unsigned short* pt = local;
            remove = firstFS;
            for(unsigned int i = 0; i < counts[j]; ++pt) {
                if(*pt == j + 1)
                    idx[i++] = first + std::distance(local, pt) - remove;
                else if(*pt == 0)
                    ++remove;
            }
            ISCreateGeneral(PETSC_COMM_WORLD, counts[j], idx, PETSC_COPY_VALUES, &is);
            PCFieldSplitSetIS(pc, names && j < names->size() ? (*(names->operator[](j))).c_str() : NULL, is);
            ISDestroy(&is);
        }
        if(mT && mT->n > 0 && (pL || std::is_same<Tab, PETSc::DistributedCSR<HpSchwarz<PetscScalar>>>::value)) {
            if(ptA->_vS) {
                for(int i = 0; i < ptA->_vS->size(); ++i)
                    MatDestroy(&(*ptA->_vS)[i]);
                delete ptA->_vS;
                ptA->_vS = nullptr;
            }
            ptA->_vS = new std::vector<Mat>();
            ptA->_vS->resize(mT->n);
            setVectorSchur(ptA, mT, pL);
        }
        delete [] idx;
        delete [] counts;
        local -= fields->n;
        delete [] local;
    }
}
void setCompositePC(PC pc, const std::vector<Mat>* S) {
    if(S) {
        PetscInt nsplits;
        KSP* subksp;
        PCFieldSplitGetSubKSP(pc, &nsplits, &subksp);
        if(S->size() == 1) {
            KSPSetOperators(subksp[nsplits - 1], (*S)[0], (*S)[0]);
            IS is;
#if !PETSC_VERSION_RELEASE
            PCFieldSplitGetISByIndex(pc, nsplits - 1, &is);
#else
            const char* prefixPC;
            PCGetOptionsPrefix(pc, &prefixPC);
            const char* prefixIS;
            KSPGetOptionsPrefix(subksp[nsplits - 1], &prefixIS);
            std::string str = std::string(prefixIS).substr((prefixPC ? std::string(prefixPC).size() : 0) + std::string("fieldsplit_").size(), std::string(prefixIS).size() - ((prefixPC ? std::string(prefixPC).size() : 0) + std::string("fieldsplit_").size() + 1));
            PCFieldSplitGetIS(pc, str.c_str(), &is);
#endif
            PetscObjectCompose((PetscObject)is, "pmat", (PetscObject)(*S)[0]);
        }
        else if(!S->empty()) {
            PC pcS;
            KSPGetPC(subksp[nsplits - 1], &pcS);
            for(int i = 0; i < S->size(); ++i)
                PCCompositeAddPC(pcS, PCNONE);
            PCSetUp(pcS);
            for(int i = 0; i < S->size(); ++i) {
                PC subpc;
                PCCompositeGetPC(pcS, i, &subpc);
                PCSetOperators(subpc, (*S)[i], (*S)[i]);
                PCSetFromOptions(subpc);
            }
        }
        PetscFree(subksp);
    }
}
bool insertOptions(std::string* const& options) {
    bool fieldsplit = false;
    if(options) {
        std::vector<std::string> elems;
        std::stringstream ss(*options);
        std::string item;
        while (std::getline(ss, item, ' '))
            elems.push_back(item);
        int argc = elems.size() + 1;
        char** data = new char*[argc];
        data[0] = new char[options->size() + argc]();
        data[1] = data[0] + 1;
        for(int i = 0; i < argc - 1; ++i) {
            if(i > 0)
                data[i + 1] = data[i] + elems[i - 1].size() + 1;
            strcpy(data[i + 1], elems[i].c_str());
            if(!fieldsplit)
                fieldsplit = (elems[i].compare("fieldsplit") == 0);
        }
        FFPetscOptionsInsert(&argc, &data, NULL);
        delete [] *data;
        delete [] data;
    }
    return fieldsplit;
}
}
#endif
