from scipy.interpolate import InterpolatedUnivariateSpline
from scipy import integrate
import param  # cosmological parameter object from Cosmopy
import numpy
import cosmology

"""Classes for encoding basic cosmological parameters and quantities.

Given a cosmology, we can derive a mass function object.  This should be able
to return the abundance of halo at a given mass as well as the mean halo bias.
Likewise, these quantities should be available as a function of nu:

nu = (delta_c/sigma(M))^2

This is the ratio of the over-density necessary for linear, spherical
collapse and the RMS density fluctuations as a function of mass.  M*, the mass
at which nu == 1, represents the break between halos that are mostly collapsed
and those where fluctuations large enough for collapse are exponentially
suppressed.
"""

__author__ = "Ryan Scranton <ryan.scranton@gmail.com>"

class MassFunction(object):
    """Object representing a mass function for a given input cosmology.

    A MassFunction object can return a properly normalized halo abundance or
    halo bias as a function of halo mass or as a function of nu, as well as
    translate between mass and nu.
    """
    def __init__(self, redshift=0.0, camb_param=None, halo_param=None, **kws):
        # Hard coded, but we shouldn't expect halos outside of this range.
        self.redshift = redshift
        self.cosmo = cosmology.SingleEpoch(self.redshift, camb_param)
        self.delta_c = self.cosmo.delta_c()

        mass_min = 1.0e8
        mass_max = 1.0e16

        mass_limit_not_set = True
        while mass_limit_not_set:
            if 0.105 < self.cosmo.nu_m(mass_min):
                #print "Min mass", mass_min,"too high..."
                mass_min = mass_min/1.05
                #print "\tSetting to",mass_min,"..."
                continue
            elif 0.095 > self.cosmo.nu_m(mass_min):
                #print "Min mass", mass_min,"too low..."
                mass_min = mass_min*1.05
                #print "\tSetting to",mass_min,"..."
                continue
            if  47.5 > self.cosmo.nu_m(mass_max):
                #print "Max mass", mass_max,"too low..."
                mass_max = mass_max*1.05
                #print "\tSetting to",mass_max,"..."
                continue
            elif 52.5 < self.cosmo.nu_m(mass_max):
                #print "Max mass", mass_max,"too high..."
                mass_max = mass_max/1.05
                #print "\tSetting to",mass_max,"..."
                continue
            mass_limit_not_set = False

        print "Mass Limits:",mass_min,"-",mass_max

        dln_mass = (numpy.log(mass_max) - numpy.log(mass_min))/100
        self.ln_mass_max = numpy.log(mass_max) + dln_mass
        self.ln_mass_min = numpy.log(mass_min) - dln_mass

        self._ln_mass_array = numpy.arange(
            self.ln_mass_min, self.ln_mass_max + dln_mass, dln_mass)

        if camb_param is None:
            camb_param = param.CambParams(**kws)

        if halo_param is None:
            halo_param = param.HaloModelParams(**kws)

        self.stq = halo_param.stq
        self.st_little_a = halo_param.st_little_a
        self.c0 = halo_param.cbarcoef/(1.0 + redshift)
        self.beta = halo_param.cbarslope
        self.alpha = halo_param.dpalpha

        self._initialize_splines()
        self._normalize()

    

    def _initialize_splines(self):
        self._nu_array = numpy.zeros_like(self._ln_mass_array)

        for idx in xrange(self._ln_mass_array.size):
            mass = numpy.exp(self._ln_mass_array[idx])
            self._nu_array[idx] = self.cosmo.nu_m(mass)

        self.nu_min = 1.001*self._nu_array[0]
        self.nu_max = 0.999*self._nu_array[-1]

        print "nu_min:",self.nu_min,"nu_max:",self.nu_max

        self._nu_spline = InterpolatedUnivariateSpline(
            self._ln_mass_array, self._nu_array)
        self._ln_mass_spline = InterpolatedUnivariateSpline(
            self._nu_array, self._ln_mass_array)

        # Set M_star, the mass for which nu == 1
        self.m_star = self.mass(1.0)

    def _normalize(self):
        self.f_norm = 1.0
        norm, norm_err = integrate.quad(self.f_nu, self.nu_min, self.nu_max,
                                        limit=200)
        self.f_norm = 1.0/norm

        self.bias_norm = 1.0
        norm, norm_err = integrate.quad(lambda x: self.f_nu(x)*self.bias_nu(x),
                                        self.nu_min, self.nu_max,
                                        limit=200)
        self.bias_norm = 1.0/norm

    def f_nu(self, nu):
        nu_prime = nu*self.st_little_a
        return (
            self.f_norm*(1.0 + nu_prime**(-1.0*self.stq))*
            numpy.sqrt(nu_prime)*numpy.exp(-0.5*nu_prime)/nu)

    def f_m(self, mass):
        return self.f_nu(self.nu(mass))

    def bias_nu(self, nu):
        """Halo bias as a function of nu."""
        nu_prime = nu*self.st_little_a
        return self.bias_norm*(
            1.0 + (nu - 1.0)/self.delta_c +
            2.0*self.stq/(self.delta_c*(1.0 + nu_prime**self.stq)))

    def bias_m(self, mass):
        """Halo bias as a function of mass."""
        return self.bias_nu(self.nu(mass))

    def nu(self, mass):
        """nu as a function of halo mass."""
        return self._nu_spline(numpy.log(mass))[0]

    def ln_mass(self, nu):
        """Natural log of halo mass as a function of nu."""
        return self._ln_mass_spline(nu)[0]

    def mass(self, nu):
        """Halo mass as a function of nu."""
        return numpy.exp(self.ln_mass(nu))

    def write(self, output_file_name):
        print "M* = 10^%1.4f M_sun" % numpy.log10(self.m_star)
        output_file = open(output_file_name, "w")
        for ln_mass, nu, in zip(self._ln_mass_array, self._nu_array):
            output_file.write("%1.10f %1.10f %1.10f %1.10f\n" % (
                numpy.exp(ln_mass), nu, self.f_nu(nu), self.bias_nu(nu)))
        output_file.close()

class TinkerMassFunction(MassFunction):

    def __init__(self, redshift=0.0, camb_param=None, halo_param=None, **kws):
        self.A0 = 0.26
        self.a0 = 2.30
        self.b0 = 1.46
        self.c  = 1.97
        self.k_min = 0.001
        self.k_max = 100.0
        self._initialized_spline = False

        MassFunction.__init__(self, redshift, camb_param, halo_param, **kws)

        self._initialize_f_nu_spline()

    def _initialize_f_nu_spline(self):
        if self._initialized_spline == False:
            self._ln_f_nu_array = numpy.empty(self._nu_array.shape)
            for idx, nu in enumerate(self._nu_array):
                self._ln_f_nu_array[idx] = numpy.log(self._f_nu(nu))
            self._ln_f_nu_spline = InterpolatedUnivariateSpline(
                self._nu_array,self._ln_f_nu_array)
            self._initialized_spline == True

    def _f_nu(self, nu):
        sigma = self.cosmo.sigma_m(self.mass(nu))
        mass = self.mass(nu)
        return (self.f_norm*self.f_sigma(sigma)*
                self.cosmo.rho_bar()/mass*(-1.0/sigma)*
                self.cosmo.sigma_m_prime(mass))

    def f_nu(self, nu):
        if self._initialized_spline == False:
            self._initialize_f_nu_spline()
        return numpy.exp(self._ln_f_nu_spline(nu))

    def f_m(self, mass):
        nu = self.nu(mass)
        return self.f_nu(nu)

    def f_sigma(self, sigma):
        return (self.A0**(numpy.power(sigma/self.b0,-self.a0) + 1)*
                numpy.exp(-self.c0/(sigma*sigma)))