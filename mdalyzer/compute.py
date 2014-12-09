import libmdalyzer

class compute(object):
    counter = 0
    def __init__(self, trajectory, file_name, name):
        self.trajectory = trajectory
        self.file_name = file_name
        
        if name is None:
            name = str(compute.counter)
            compute.counter += 1
        self.name = name

class density(compute):
    """Density profile compute"""
    
    def __init__(self, trajectory, file_name='density', nx=0, ny=0, nz=0, name=None, types=[], weight=True):
        compute.__init__(self, trajectory, file_name, name)
        self.bins = libmdalyzer.Vector3uint(nx,ny,nz)
        
        self.cpp = libmdalyzer.DensityProfile(self.trajectory.cpp, self.file_name, self.bins)
        self.trajectory.cpp.addCompute(self.cpp, self.name)
        
        self.types = []
        self.weight = True
        
        
        if not isinstance(types, list):
            types = [types]
        self.add_type(types)
        self.mass_weight(weight)
    
    def construct(self, traj):
        """Construct the C++ object with the Trajectory"""
        self.cpp = libmdalyzer.DensityProfile(traj, self.file_name, self.bins)
        for t in self.types:
            self.cpp.addType(t)
        self.cpp.useMassWeighting(self.weight)
    
    def add_type(self, types):
        """Add types to calculate"""
        if not isinstance(types, list):
            types = [types]
            
        for t in types:
            if t not in self.types:
                self.types += [t]
                self.cpp.addType(t)
    
    def delete_type(self, types):
        """Remove types to calculate"""
        if not isinstance(types, list):
            types = [types]
            
        for t in types:
            self.types.remove(t)
            self.cpp.deleteType(t)
    
    def mass_weight(self, weight):
        """Set mass weighting on or off"""
        if weight is None:
            weight = False
        
        if not isinstance(weight, bool):
            raise RuntimeError('Mass weighting must be True or False')
        
        self.weight = weight
        self.cpp.useMassWeighting(self.weight)
    
    