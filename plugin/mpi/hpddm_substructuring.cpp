//ff-c++-LIBRARY-dep: cxx11 hpddm [mumps parmetis ptscotch scotch scalapack|umfpack] [mkl|blas] mpi pthread mpifc fc
//ff-c++-cpp-dep:

#define HPDDM_SCHWARZ 0
#define HPDDM_FETI    1
#define HPDDM_BDD     1

#include "common.hpp"

namespace Substructuring {
class Skeleton_Op : public E_F0mps {
    public:
        Expression interface;
        Expression restriction;
        Expression outInterface;
        static const int n_name_param = 3;
        static basicAC_F0::name_and_type name_param[];
        Expression nargs[n_name_param];
        Skeleton_Op(const basicAC_F0& args, Expression param1, Expression param2, Expression param3) : interface(param1), restriction(param2), outInterface(param3) {
            args.SetNameParam(n_name_param, name_param, nargs);
        }
        AnyType operator()(Stack stack) const;
};
basicAC_F0::name_and_type Skeleton_Op::name_param[] = {
    {"communicator", &typeid(pcommworld)},
    {"interface", &typeid(KN<long>*)},
    {"redundancy", &typeid(bool)}
};
class Skeleton : public OneOperator {
    public:
        Skeleton() : OneOperator(atype<long>(), atype<KN<double>*>(), atype<KN<Matrice_Creuse<double> >*>(), atype<KN<KN<long> >*>()) {}

        E_F0* code(const basicAC_F0& args) const
        {
            return new Skeleton_Op(args, t[0]->CastTo(args[0]), t[1]->CastTo(args[1]), t[2]->CastTo(args[2]));
        }
};
AnyType Skeleton_Op::operator()(Stack stack) const {
    KN<double>* in = GetAny<KN<double>*>((*interface)(stack));
    KN<KN<long> >* out = GetAny<KN<KN<long> >*>((*outInterface)(stack));
    KN<Matrice_Creuse<double> >* interpolation = GetAny<KN<Matrice_Creuse<double> >*>((*restriction)(stack));
    MPI_Comm* comm = nargs[0] ? (MPI_Comm*)GetAny<pcommworld>((*nargs[0])(stack)) : 0;
    unsigned short n = out->operator[](0).n;
    MPI_Request* rq = new MPI_Request[2 * n];
    std::vector<unsigned char*> send(n);
    std::vector<unsigned char*> recv(n);
    unsigned short neighborAfter = 0;
    if(out->n != 1 + n)
        out->resize(1 + n);
    for(unsigned short i = 0; i < n; ++i) {
        MatriceMorse<double>* pt = static_cast<MatriceMorse<double>*>(&(*interpolation->operator[](i).A));
#ifdef VERSION_MATRICE_CREUSE
        pt->CSR();
#endif
        send[i] = new unsigned char[pt->n];
        recv[i] = new unsigned char[pt->n];
        unsigned int dest = out->operator[](0).operator[](i);
        if(dest < mpirank) {
            unsigned int col = 0;
            for(unsigned int j = 0; j < pt->n; ++j) {
#ifndef VERSION_MATRICE_CREUSE
                if(pt->lg[j + 1] != pt->lg[j]) {
                    if(std::abs(in->operator[](pt->cl[col++]) - 1.0) < 0.1)
#else
                if(pt->p[j + 1] != pt->p[j]) {
                    if(std::abs(in->operator[](pt->j[col++]) - 1.0) < 0.1)
#endif
                        send[i][j] = '1';
                    else
                        send[i][j] = '0';
                }
                else
                    send[i][j] = '0';
            }
            MPI_Isend(send[i], pt->n, MPI_UNSIGNED_CHAR, dest, 0, *comm, rq + i);
            ++neighborAfter;
        }
        else
            MPI_Irecv(recv[i], pt->n, MPI_UNSIGNED_CHAR, dest, 0, *comm, rq + i);
    }
    for(unsigned short i = 0; i < neighborAfter; ++i) {
        MatriceMorse<double>* pt = static_cast<MatriceMorse<double>*>(&(*interpolation->operator[](i).A));
        // cout << mpirank << " receives from " << arrayNeighbor->operator[](i) << ", " << pt->n << "." << endl;
        MPI_Irecv(recv[i], pt->n, MPI_UNSIGNED_CHAR, out->operator[](0).operator[](i), 0, *comm, rq + n + i);
    }
    for(unsigned short i = neighborAfter; i < n; ++i) {
        int index;
        MPI_Waitany(n - neighborAfter, rq + neighborAfter, &index, MPI_STATUS_IGNORE);
        unsigned short dest = neighborAfter + index;
        MatriceMorse<double>* pt = static_cast<MatriceMorse<double>*>(&(*interpolation->operator[](dest).A));
        KN<long>& resOut = out->operator[](1 + dest);
       

#ifdef VERSION_MATRICE_CREUSE
        pt->CSR();
         resOut.resize(pt->nnz);
#else
         resOut.resize(pt->nbcoef);
#endif

        unsigned int nnz = 0;
        unsigned int col = 0;
        for(unsigned int j = 0; j < pt->n; ++j) {
#ifndef VERSION_MATRICE_CREUSE
            if(pt->lg[j + 1] != pt->lg[j]) {
                if(std::abs(in->operator[](pt->cl[col]) - 1.0) < 0.1 && recv[dest][j] == '1') {
                    send[dest][j] = '1';
                    resOut[(int)nnz++] = pt->cl[col++];
                }
#else
            if(pt->p[j + 1] != pt->p[j]) {
                 if(std::abs(in->operator[](pt->j[col]) - 1.0) < 0.1 && recv[dest][j] == '1') {
                     send[dest][j] = '1';
                     resOut[(int)nnz++] = pt->j[col++];
                 }
#endif
 
                else {
                    ++col;
                    send[dest][j] = '0';
                }
            }
            else
                send[dest][j] = '0';
        }
        // cout << mpirank << " sends to " << arrayNeighbor->operator[](dest) << ", " << pt->n << "." << endl;
        MPI_Isend(send[dest], pt->n, MPI_UNSIGNED_CHAR, out->operator[](0).operator[](dest), 0, *comm, rq + n + dest);
        resOut.resize(nnz);
    }
    for(unsigned short i = 0; i < neighborAfter; ++i) {
        int index;
        MPI_Waitany(neighborAfter, rq + n, &index, MPI_STATUS_IGNORE);
        KN<long>& resOut = out->operator[](1 + index);
        MatriceMorse<double>* pt = static_cast<MatriceMorse<double>*>(&(*interpolation->operator[](index).A));
#ifndef VERSION_MATRICE_CREUSE
        resOut.resize(pt->nbcoef);
#else
        resOut.resize(pt->nnz);
#endif
        unsigned int nnz = 0;
        unsigned int col = 0;
        for(unsigned int j = 0; j < pt->n; ++j) {
            if(recv[index][j] == '1') {
#ifndef VERSION_MATRICE_CREUSE
                if(pt->lg[j + 1] != pt->lg[j])
                    resOut[(int)nnz++] = pt->cl[col++];
            }
           else if(pt->lg[j + 1] != pt->lg[j])
                ++col;
#else
               if(pt->p[j + 1] != pt->p[j])
                resOut[(int)nnz++] = pt->j[col++];
           }
             else if(pt->p[j + 1] != pt->p[j])
               ++col;
#endif
        }
        resOut.resize(nnz);
    }
    MPI_Waitall(neighborAfter, rq, MPI_STATUSES_IGNORE);
    MPI_Waitall(n - neighborAfter, rq + n + neighborAfter, MPI_STATUSES_IGNORE);
    for(unsigned short i = 0; i < n; ++i) {
        delete [] recv[i];
        delete [] send[i];
        // cout << mpirank << " <=> " << arrayNeighbor->operator[](i) << " : " << out->operator[](i).n << endl;
    }
    delete [] rq;
    KN<long>* interfaceNb = nargs[1] ? GetAny<KN<long>* >((*nargs[1])(stack)) : (KN<long>*) 0;
    if(interfaceNb) {
        std::vector<unsigned int> vec;
        vec.reserve(in->n);
        for(int i = 0; i < in->n; ++i) {
            if(in->operator[](i) != 0.0)
                vec.emplace_back(i);
        }
        std::sort(vec.begin(), vec.end());
        if(interfaceNb->n != vec.size())
            interfaceNb->resize(vec.size());
        for(  signed int i = 0; i < vec.size(); ++i)
            interfaceNb->operator[](i) = vec[i];
        for(unsigned short i = 0; i < n; ++i) {
            KN<long>& res = out->operator[](1 + i);
            for(  signed int j = 0; j < res.n; ++j) {
                std::vector<unsigned int>::const_iterator idx = std::lower_bound(vec.cbegin(), vec.cend(), (unsigned int)res[j]);
                if(idx == vec.cend() || res[j] < *idx) {
                    std::cout << "Problem !" << std::endl;
                    res[j] = -1;
                }
                else
                    res[j] = std::distance(vec.cbegin(), idx);
            }
        }
        bool redundancy = nargs[2] ? GetAny<bool>((*nargs[2])(stack)) : 1;
        if(!redundancy) {
            std::vector<std::pair<unsigned short, unsigned int> >* array = new std::vector<std::pair<unsigned short, unsigned int> >[interfaceNb->n];
            for(unsigned short i = 0; i < n; ++i) {
                KN<long>& res = out->operator[](1 + i);
                for(  signed int j = 0; j < res.n; ++j)
                    array[res[j]].push_back(std::make_pair(i, res[j]));
            }
            for(unsigned int i = 0; i < interfaceNb->n; ++i) {
                if(array[i].size() > 1) {
                    if(mpirank > out->operator[](0).operator[](array[i].back().first))
                        array[i].erase(array[i].begin());
                    else if(mpirank < out->operator[](0).operator[](array[i].front().first))
                        array[i].pop_back();
                }
                else if (array[i].size() < 1)
                    std::cout << "Problem !" << std::endl;
            }
            std::vector<long>* copy = new std::vector<long>[n];
            for(unsigned short i = 0; i < n; ++i)
                copy[i].reserve(out->operator[](1 + i).n);
            for(unsigned int i = 0; i < interfaceNb->n; ++i) {
                for(std::vector<std::pair<unsigned short, unsigned int> >::const_iterator it = array[i].cbegin(); it != array[i].cend(); ++it) {
                    copy[it->first].push_back(it->second);
                }
            }
            for(unsigned short i = 0; i < n; ++i) {
                unsigned int sizeVec = copy[i].size();
                if(sizeVec != out->operator[](1 + i).n) {
                    out->operator[](1 + i).resize(sizeVec);
                    long* pt = (static_cast<KN_<long> >(out->operator[](1 + i)));
                    std::reverse_copy(copy[i].begin(), copy[i].end(), pt);
                }
            }
            delete [] copy;
            delete [] array;
        }
    }
    return 0L;
}


template<class Type, class K>
class initDDM_Op : public E_F0mps {
    public:
        Expression A;
        Expression Mat;
        Expression R;
        static const int n_name_param = 2;
        static basicAC_F0::name_and_type name_param[];
        Expression nargs[n_name_param];
        initDDM_Op(const basicAC_F0& args, Expression param1, Expression param2, Expression param3) : A(param1), Mat(param2), R(param3) {
            args.SetNameParam(n_name_param, name_param, nargs);
        }

        AnyType operator()(Stack stack) const;
};
template<class Type, class K>
basicAC_F0::name_and_type initDDM_Op<Type, K>::name_param[] = {
    {"communicator", &typeid(pcommworld)},
    {"deflation", &typeid(FEbaseArrayKn<K>*)},
};
template<class Type, class K>
class initDDM : public OneOperator {
    public:
        initDDM() : OneOperator(atype<Type*>(), atype<Type*>(), atype<Matrice_Creuse<K>*>(), atype<KN<KN<long>>*>()) { }

        E_F0* code(const basicAC_F0& args) const {
            return new initDDM_Op<Type, K>(args, t[0]->CastTo(args[0]), t[1]->CastTo(args[1]), t[2]->CastTo(args[2]));
        }
};
template<class Type, class K>
AnyType initDDM_Op<Type, K>::operator()(Stack stack) const {
    Type* ptA = GetAny<Type*>((*A)(stack));
    Matrice_Creuse<K>* pA = GetAny<Matrice_Creuse<K>*>((*Mat)(stack));
    MatriceMorse<K>* mA = pA->A ? static_cast<MatriceMorse<K>*>(&(*pA->A)) : nullptr;
    KN<KN<long>>* ptR = GetAny<KN<KN<long>>*>((*R)(stack));
    if(ptR) {
        KN_<KN<long>> sub(ptR->n > 0 && ptR->operator[](0).n > 0 ? (*ptR)(FromTo(1, ptR->n - 1)) : KN<KN<long>>());
        ptA->HPDDM::template Subdomain<K>::initialize(new_HPDDM_MatrixCSR<K>(mA), STL<long>(ptR->n > 0 ? ptR->operator[](0) : KN<long>()), sub, nargs[0] ? (MPI_Comm*)GetAny<pcommworld>((*nargs[0])(stack)) : 0);
    }
    FEbaseArrayKn<K>* deflation = nargs[1] ? GetAny<FEbaseArrayKn<K>*>((*nargs[1])(stack)) : 0;
    if(deflation && deflation->N > 0 && !ptA->getVectors()) {
        K** ev = new K*[deflation->N];
        *ev = new K[deflation->N * deflation->get(0)->n];
        for(int i = 0; i < deflation->N; ++i) {
            ev[i] = *ev + i * deflation->get(0)->n;
            std::copy(&(*deflation->get(i))[0], &(*deflation->get(i))[deflation->get(i)->n], ev[i]);
        }
        ptA->setVectors(ev);
        ptA->Type::super::super::initialize(deflation->N);
    }
    return ptA;
}

template<class Type, class K>
class attachCoarseOperator_Op : public E_F0mps {
    public:
        Expression comm;
        Expression A;
        static const int n_name_param = 4;
        static basicAC_F0::name_and_type name_param[];
        Expression nargs[n_name_param];
        attachCoarseOperator_Op(const basicAC_F0& args, Expression param1, Expression param2) : comm(param1), A(param2) {
            args.SetNameParam(n_name_param, name_param, nargs);
        }

        AnyType operator()(Stack stack) const;
};
template<class Type, class K>
basicAC_F0::name_and_type attachCoarseOperator_Op<Type, K>::name_param[] = {
    {"R", &typeid(FEbaseArrayKn<K>*)},
    {"threshold", &typeid(HPDDM::underlying_type<K>)},
    {"timing", &typeid(KN<double>*)},
    {"ret", &typeid(Pair<K>*)}
};
template<class Type, class K>
class attachCoarseOperator : public OneOperator {
    public:
        attachCoarseOperator() : OneOperator(atype<long>(), atype<pcommworld>(), atype<Type*>()) { }

        E_F0* code(const basicAC_F0& args) const {
            return new attachCoarseOperator_Op<Type, K>(args, t[0]->CastTo(args[0]), t[1]->CastTo(args[1]));
        }
};
template<class Type, class K>
AnyType attachCoarseOperator_Op<Type, K>::operator()(Stack stack) const {
    pcommworld ptComm = GetAny<pcommworld>((*comm)(stack));
    MPI_Comm comm = *(MPI_Comm*)ptComm;
    Type* ptA = GetAny<Type*>((*A)(stack));
    FEbaseArrayKn<K>* R = nargs[0] ? GetAny<FEbaseArrayKn<K>*>((*nargs[0])(stack)) : 0;
    Pair<K>* pair = nargs[3] ? GetAny<Pair<K>*>((*nargs[3])(stack)) : 0;
    unsigned short nu = R ? static_cast<unsigned short>(R->N) : 0;
    HPDDM::Option& opt = *HPDDM::Option::get();
    HPDDM::underlying_type<K> threshold = opt.val("geneo_threshold", 0.0);
    KN<double>* timing = nargs[2] ? GetAny<KN<double>*>((*nargs[2])(stack)) : 0;
    std::pair<MPI_Request, const K*>* ret = nullptr;
    bool adaptive = opt.set("geneo_nu") || threshold > 0.0;
    if(!adaptive)
        ptA->setDeficiency(nu);
    double t = MPI_Wtime();
    if(R) {
        if(adaptive)
            ptA->computeSchurComplement();
        ptA->callNumfactPreconditioner();
        if(timing)
            (*timing)[3] = MPI_Wtime() - t;
        if(adaptive) {
            if(opt.set("geneo_nu"))
                nu = opt["geneo_nu"];
#if defined(MUMPSSUB) || defined(PASTIXSUB) || defined(MKL_PARDISOSUB)
            t = MPI_Wtime();
            ptA->solveGEVP();
            if(timing)
                (*timing)[5] = MPI_Wtime() - t;
#else
            cout << "Please change your solver" << endl;
#endif
        }
        if(!R && !ptA->getVectors())
            cout << "Problem !" << endl;
        R->resize(0);
        MPI_Barrier(MPI_COMM_WORLD);
        if(timing)
            t = MPI_Wtime();
        if(ptA->exclusion(comm)) {
            if(pair)
                pair->p = ptA->template buildTwo<1>(comm);
            else
                ret = ptA->template buildTwo<1>(comm);
        }
        else {
            if(pair)
                pair->p = ptA->template buildTwo<0>(comm);
            else
                ret = ptA->template buildTwo<0>(comm);
        }
        if(timing)
            (*timing)[4] = MPI_Wtime() - t;
        if(pair)
            if(pair->p) {
                int flag;
                MPI_Test(&(pair->p->first), &flag, MPI_STATUS_IGNORE);
            }
        t = MPI_Wtime();
        ptA->callNumfact();
    }
    else {
        MPI_Barrier(MPI_COMM_WORLD);
        ret = ptA->template buildTwo<2>(comm);
    }
    if(timing)
        (*timing)[2] = MPI_Wtime() - t;
    if(ret)
        delete ret;
    if(pair)
        if(pair->p) {
            if(timing)
                t = MPI_Wtime();
            MPI_Wait(&(pair->p->first), MPI_STATUS_IGNORE);
            if(timing)
                (*timing)[timing->n - 1] = MPI_Wtime() - t;
            delete [] pair->p->second;
            pair->destroy();
            pair = nullptr;
        }
    return 0L;
}

template<class Type, class K>
class solveDDM_Op : public E_F0mps {
    public:
        Expression A;
        Expression rhs;
        Expression x;
        static const int n_name_param = 3;
        static basicAC_F0::name_and_type name_param[];
        Expression nargs[n_name_param];
        solveDDM_Op(const basicAC_F0& args, Expression param1, Expression param2, Expression param3) : A(param1), rhs(param2), x(param3) {
            args.SetNameParam(n_name_param, name_param, nargs);
        }

        AnyType operator()(Stack stack) const;
};
template<class Type, class K>
basicAC_F0::name_and_type solveDDM_Op<Type, K>::name_param[] = {
    {"timing", &typeid(KN<double>*)},
    {"excluded", &typeid(bool)},
    {"ret", &typeid(Pair<K>*)}
};
template<class Type, class K>
class solveDDM : public OneOperator {
    public:
        solveDDM() : OneOperator(atype<long>(), atype<Type*>(), atype<KN<K>*>(), atype<KN<K>*>()) { }

        E_F0* code(const basicAC_F0& args) const {
            return new solveDDM_Op<Type, K>(args, t[0]->CastTo(args[0]), t[1]->CastTo(args[1]), t[2]->CastTo(args[2]));
        }
};
template<class Type, class K>
AnyType solveDDM_Op<Type, K>::operator()(Stack stack) const {
    KN<K>* ptRHS = GetAny<KN<K>*>((*rhs)(stack));
    KN<K>* ptX = GetAny<KN<K>*>((*x)(stack));
    Type* ptA = GetAny<Type*>((*A)(stack));
    if(ptX->n != ptRHS->n)
        return 0L;
    HPDDM::Option& opt = *HPDDM::Option::get();
    KN<double>* timing = nargs[0] ? GetAny<KN<double>*>((*nargs[0])(stack)) : 0;
    bool excluded = nargs[1] && GetAny<bool>((*nargs[1])(stack));
    if(excluded)
        opt["level_2_exclude"];
    double timer = MPI_Wtime();
    if(mpisize == 1) {
        ptA->computeSchurComplement();
        ptA->callNumfactPreconditioner();
        if(timing)
            (*timing)[3] = MPI_Wtime() - timer;
        timer = MPI_Wtime();
        ptA->callNumfact();
        if(timing)
            (*timing)[2] = MPI_Wtime() - timer;
    }
    MPI_Barrier(MPI_COMM_WORLD);
    if(!excluded && timing && mpisize > 1)
        (*timing)[timing->n - 1] += MPI_Wtime() - timer;
    timer = MPI_Wtime();
    int rank;
    MPI_Comm_rank(ptA->getCommunicator(), &rank);
    if(rank != mpirank || rank != 0)
        opt.remove("verbosity");
    timer = MPI_Wtime();
    if(!excluded)
        HPDDM::IterativeMethod::solve(*ptA, (K*)*ptRHS, (K*)*ptX, 1, MPI_COMM_WORLD);
    else
        HPDDM::IterativeMethod::solve<true>(*ptA, (K*)nullptr, (K*)nullptr, 1, MPI_COMM_WORLD);
    timer = MPI_Wtime() - timer;
    if(!excluded && verbosity > 0 && rank == 0)
        std::cout << scientific << " --- system solved (in " << timer << ")" << std::endl;
    return 0L;
}

template<class Type, class K>
class renumber_Op : public E_F0mps {
    public:
        Expression A;
        Expression Mat;
        Expression interface;
        static const int n_name_param = 4;
        static basicAC_F0::name_and_type name_param[];
        Expression nargs[n_name_param];
        renumber_Op(const basicAC_F0& args, Expression param1, Expression param2, Expression param3) : A(param1), Mat(param2), interface(param3) {
            args.SetNameParam(n_name_param, name_param, nargs);
        }

        AnyType operator()(Stack stack) const;
};
template<class Type, class K>
basicAC_F0::name_and_type renumber_Op<Type, K>::name_param[] = {
    {"R", &typeid(FEbaseArrayKn<K>*)},
    {"effort", &typeid(KN<K>*)},
    {"rho", &typeid(KN<K>*)},
    {"timing", &typeid(KN<double>*)}
};
template<class Type, class K>
class renumber : public OneOperator {
    public:
        renumber() : OneOperator(atype<long>(), atype<Type*>(), atype<Matrice_Creuse<K>*>(), atype<KN<long>*>()) { }

        E_F0* code(const basicAC_F0& args) const {
            return new renumber_Op<Type, K>(args, t[0]->CastTo(args[0]), t[1]->CastTo(args[1]), t[2]->CastTo(args[2]));
        }
};
    template<class Type, class K>
    AnyType renumber_Op<Type, K>::operator()(Stack stack) const {
        Type* ptA = GetAny<Type*>((*A)(stack));
        KN<long>* ptInterface = GetAny<KN<long>*>((*interface)(stack));
        FEbaseArrayKn<K>* deflation = nargs[0] ? GetAny<FEbaseArrayKn<K>*>((*nargs[0])(stack)) : 0;
        KN<K>* ptEffort = nargs[1] ? GetAny<KN<K>*>((*nargs[1])(stack)) : 0;
        KN<K>* rho = nargs[2] ? GetAny<KN<K>*>((*nargs[2])(stack)) : 0;
        KN<double>* timing = nargs[3] ? GetAny<KN<double>*>((*nargs[3])(stack)) : 0;
        double t = MPI_Wtime();
        K** ev;
        if(deflation && deflation->N > 0) {
            ev = new K*[deflation->N];
            *ev = new K[deflation->N * deflation->get(0)->n];
            for(int i = 0; i < deflation->N; ++i) {
                ev[i] = *ev + i * deflation->get(0)->n;
                std::copy(static_cast<K*>(*(deflation->get(i))), static_cast<K*>(*(deflation->get(i))) + deflation->get(i)->n, ev[i]);
            }
            ptA->setVectors(ev);
            ptA->Type::super::super::initialize(deflation->N);
        }

        ptA->renumber(STL<long>(*ptInterface), ptEffort ? static_cast<K*>(*ptEffort) : nullptr);
        MatriceMorse<K>* mA = static_cast<MatriceMorse<K>*>(&(*GetAny<Matrice_Creuse<K>*>((*Mat)(stack))->A));
        if(mA) {
            const HPDDM::MatrixCSR<K>* dA = ptA->getMatrix();
            
    #ifndef VERSION_MATRICE_CREUSE
            mA->lg = dA->_ia;
    #else
            set_ff_matrix<K>(mA,*dA);
    #endif
        }

    HPDDM::Option& opt = *HPDDM::Option::get();
    char scaling = opt.val<char>("substructuring_scaling", 0);
    if(scaling == 2 && rho) {
        ptA->renumber(STL<long>(*ptInterface), *rho);
        ptA->buildScaling(scaling, *rho);
    }
    else
        ptA->buildScaling(scaling);

    if(timing) {
        (*timing)[1] = MPI_Wtime() - t;
        t = MPI_Wtime();
    }

    if(deflation && deflation->N > 0)
        for(int i = 0; i < deflation->N; ++i)
            std::copy(ev[i], ev[i] + deflation->get(i)->n, static_cast<K*>(*(deflation->get(i))));
    return 0L;
}

template<class Type>
long nbMult(Type* const& A) {
    return A->getMult();
}
template<class Type>
double nbDof(Type* const& A) {
    return static_cast<double>(A->getAllDof());
}
template<class Type, class K>
long originalNumbering(Type* const& A, KN<K>* const& in, KN<long>* const& interface) {
    A->originalNumbering(STL<long>(*interface), *in);
    return 0;
}

template<class T, class U, class K, char trans>
class InvSubstructuring {
    public:
        const T t;
        const U u;
        InvSubstructuring(T v, U w) : t(v), u(w) {}
        void solve(U out) const {
            if(out->n != u->n)
                return;
            if(mpisize == 1) {
                (*t).computeSchurComplement();
                (*t).callNumfactPreconditioner();
                (*t).callNumfact();
            }
            HPDDM::Option& opt = *HPDDM::Option::get();
            if(mpirank != 0)
                opt.remove("verbosity");
            HPDDM::IterativeMethod::solve(*t, (K*)*u, (K*)*out, 1, MPI_COMM_WORLD);
        }
        static U inv(U Ax, InvSubstructuring<T, U, K, trans> A) {
            A.solve(Ax);
            return Ax;
        }
        static U init(U Ax, InvSubstructuring<T, U, K, trans> A) {
            Ax->init(A.u->n);
            return Ax;
        }
};

template<class K, char S>
using HpFetiPrec = HpFeti<HPDDM::FetiPrcndtnr::DIRICHLET, K, S>;
template<template<class, char> class Type, class K, char S, char U = S>
void add() {
    Dcl_Type<Type<K, S>*>(Initialize<Type<K, S>>, Delete<Type<K, S>>);
    if(std::is_same<K, HPDDM::underlying_type<K>>::value) {
        static_assert(std::is_same<Type<K, S>, HpBdd<K, S>>::value || std::is_same<Type<K, S>, HpFetiPrec<K, S>>::value, "What are you trying to do?");
        if(std::is_same<Type<K, S>, HpBdd<K, S>>::value)
            zzzfff->Add("bdd", atype<Type<K, S>*>());
        else
            zzzfff->Add("feti", atype<Type<K, S>*>());
    }
    map_type_of_map[make_pair(atype<Type<HPDDM::underlying_type<K>, U>*>(), atype<K*>())] = atype<Type<K, S>*>();

    TheOperators->Add("<-", new initDDM<Type<K, S>, K>);
    Global.Add("attachCoarseOperator", "(", new attachCoarseOperator<Type<K, S>, K>);
    Global.Add("DDM", "(", new solveDDM<Type<K, S>, K>);
    Global.Add("renumber", "(", new renumber<Type<K, S>, K>);
    Global.Add("nbDof", "(", new OneOperator1_<double, Type<K, S>*>(nbDof));
    Global.Add("nbMult", "(", new OneOperator1_<long, Type<K, S>*>(nbMult));
    Global.Add("originalNumbering", "(", new OneOperator3_<long, Type<K, S>*, KN<K>*, KN<long>*>(originalNumbering));
    addInv<Type<K, S>, InvSubstructuring, KN<K>, K>();
    Global.Add("statistics", "(", new OneOperator1_<bool, Type<K, S>*>(statistics<Type<K, S>>));
    Global.Add("exchange", "(", new exchangeInOut<Type<K, S>, K>);

    if(!exist_type<Pair<K>*>()) {
        Dcl_Type<Pair<K>*>(InitP<Pair<K>>, Destroy<Pair<K>>);
        map_type_of_map[make_pair(atype<Pair<HPDDM::underlying_type<K>>*>(), atype<K*>())] = atype<Pair<K>*>();
    }
    aType t;
    int r;
    if(!zzzfff->InMotClef("pair", t, r) && std::is_same<K, HPDDM::underlying_type<K>>::value)
        zzzfff->Add("pair", atype<Pair<K>*>());
}
}

static void Init_Substructuring() {
    Init_Common();
    Global.Add("buildSkeleton", "(", new Substructuring::Skeleton);
#if defined(DSUITESPARSE) || defined(DHYPRE)
    constexpr char ds = 'G';
#else
    constexpr char ds = 'S';
#endif
    constexpr char zs = 'G';
    Substructuring::add<HpBdd, double, ds>();
#ifndef DHYPRE
    Substructuring::add<HpBdd, std::complex<double>, zs, ds>();
    // Substructuring::add<HpBdd, float, ds>();
    // Substructuring::add<HpBdd, std::complex<float>, zs>();
#endif
    Substructuring::add<Substructuring::HpFetiPrec, double, ds>();
#ifndef DHYPRE
    Substructuring::add<Substructuring::HpFetiPrec, std::complex<double>, zs, ds>();
    // Substructuring::add<HpFetiPrec, float, ds>();
    // Substructuring::add<HpFetiPrec, std::complex<float>, zs>();
#endif
}

LOADFUNC(Init_Substructuring)
