(= Particle (class
  :ax nil  ; x acceleration
  :ay nil  ; y acceleration
  :dur nil ; particle duration

  :step (func ()
    (super':move :ax :ay)
    (= :ax (* :ax FRICTION)
       :ay (* :ay FRICTION)
    )
    (super':step)
    (if (<= :dur 0)
        (super':kill)
        (= :dur (- :dur 1))
    )
  )

  init (func (self x y a dur)
    (= super (Actor x y 0 t)
       :ax (* 1 (cos a))
       :ay (* 1 (sin a))
       :dur (+ dur (random (/ dur 2)))
       particles (cons self particles)
    )
  )
))
